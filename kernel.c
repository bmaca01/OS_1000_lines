#include "kernel.h"
#include "common.h"
#include "memory.h"
#include "virtio.h"
#include "fs.h"
#include "process.h"
#include "trap.h"

/* Linker-provided symbols */
extern char __bss[], __bss_end[], __stack_top[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

/* SBI (Supervisor Binary Interface) functions */
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2 /* Console Getchar */);
    return ret.error;
}

/* Kernel initialization and main loop */
void kernel_main(void) {
    // Clear BSS section
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    printf("\n\n");

    // Set up trap vector
    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    // Initialize subsystems
    virtio_blk_init();
    fs_init();

    // Test disk I/O
    char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, false);
    printf("first sector: %s\n", buf);

    strcpy(buf, "hello from kernel!\n");
    read_write_disk(buf, 0, true);

    // Create idle process and shell process
    idle_proc = create_process(NULL, 0);
    idle_proc->pid = 0;
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    // Yield to first process
    yield();
    PANIC("switched to idle process");
}

/**
 * Boot entry point.
 * This is the first code that runs when the kernel is loaded.
 * Sets up the stack and jumps to kernel_main.
 */
__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "j kernel_main\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}