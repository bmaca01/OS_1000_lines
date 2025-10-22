#include "virtio.h"
#include "memory.h"

/* VirtIO block device state */
static struct virtio_virtq *blk_request_vq;
static struct virtio_blk_req *blk_req;
static paddr_t blk_req_paddr;
static uint64_t blk_capacity;

/* VirtIO register access helpers */
static uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset));
}

static uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *) (VIRTIO_BLK_PADDR + offset));
}

static void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset)) = value;
}

static void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

/* Virtqueue management */
static struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq),
                                      PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;

    // 1. Select the queue writing its index to QueueSel
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 5. Notify device about queue size
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 6. Notify device about used alignment
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    // 7. Write the physical num of the first page of the queue
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}

/**
 * Notifies the device of a new request by updating the available ring
 * and kicking the queue notify register.
 */
static void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

/**
 * Returns true if there are outstanding requests being processed.
 */
static bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

void virtio_blk_init(void) {
    // Verify device identity
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device id");

    // Device initialization sequence (virtio spec 3.1.1)
    // 1. Reset the device
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set ACKNOWLEDGE status bit
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set DRIVER status bit
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set FEATURES_OK status bit
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
    // 7. Device-specific setup, including discovery of virtqueues
    blk_request_vq = virtq_init(0);
    // 8. Set DRIVER_OK status bit
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Read disk capacity from device config space
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", (uint32_t)blk_capacity);

    // Allocate a region to store requests to the device
    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *) blk_req_paddr;
}

void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
               sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // Construct request according to virtio-blk spec
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Construct virtqueue descriptors (3-part descriptor chain)
    struct virtio_virtq *vq = blk_request_vq;
    // Descriptor 0: Request header (type, sector)
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    // Descriptor 1: Data buffer (read or write)
    vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    // Descriptor 2: Status byte (device writes result here)
    vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // Notify device of new request
    virtq_kick(vq, 0);

    // Wait until device finishes processing (busy-wait)
    while (virtq_is_busy(vq))
        ;

    // Check status: 0 = success, non-zero = error
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
                sector, blk_req->status);
        return;
    }

    // Copy data from device buffer on reads
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}