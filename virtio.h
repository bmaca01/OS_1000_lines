#pragma once
#include "common.h"
#include "kernel.h"

/**
 * Initializes the virtio-blk device.
 * Sets up the virtqueue and reads device capacity.
 */
void virtio_blk_init(void);

/**
 * Reads from or writes to the virtio-blk device.
 *
 * @param buf - Buffer to read into or write from (must be SECTOR_SIZE bytes)
 * @param sector - Sector number to read/write
 * @param is_write - true for write operation, false for read
 */
void read_write_disk(void *buf, unsigned sector, int is_write);