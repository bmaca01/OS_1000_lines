# RISC-V Minimal Operating System

A pedagogical bare-metal operating system kernel for RISC-V 32-bit architecture, designed to demonstrate fundamental OS concepts in under 1000 lines of code.

> **Note:** This is a refactored version of [Seiya Nuta's "Operating System in 1000 Lines"](https://github.com/nuta/operating-system-in-1000-lines). The original codebase has been reorganized into modules for improved readability and maintainability while preserving all original functionality.
>
> **Original Tutorial:** https://1000os.seiya.me/en/

## Table of Contents

- [Overview](#overview)
- [Core Operating System Principles](#core-operating-system-principles)
- [Architecture & Design Decisions](#architecture--design-decisions)
- [Module Guide](#module-guide)
- [Building and Running](#building-and-running)
- [Learning Path](#learning-path)

## Overview

This kernel implements a minimal but functional operating system that runs directly on RISC-V hardware (or in QEMU). It demonstrates:

- **Process Management:** Cooperative multitasking with process isolation
- **Memory Management:** Virtual memory with SV32 paging
- **I/O System:** VirtIO block device driver
- **File System:** TAR-based persistent storage
- **System Calls:** User-kernel communication interface

**Key Constraint:** Freestanding environment (no standard library) - everything is implemented from scratch.

### Refactoring Changes

The original monolithic `kernel.c` (678 lines) has been split into focused modules:

- `memory.c/h` - Memory management (36 lines)
- `virtio.c/h` - Block device driver (157 lines)
- `fs.c/h` - TAR-based filesystem (107 lines)
- `process.c/h` - Process scheduling (162 lines)
- `trap.c/h` - Exception/syscall handling (168 lines)
- `kernel.c` - Boot and initialization (90 lines)

Additional improvements:
- Added `PROC_STACK_SIZE` constant
- Added `strlen()` to common library
- Enhanced documentation with function-level comments
- Removed dead code and test artifacts

## Core Operating System Principles

### 1. Privilege Separation

**Principle:** The OS must separate privileged kernel code from unprivileged user code to ensure system stability and security.

**Implementation:**
- **Supervisor Mode (S-mode):** Kernel runs with full hardware access
- **User Mode (U-mode):** Applications run with restricted privileges
- **Page Tables:** Hardware enforces memory isolation (`PAGE_U` flag)

**Why it matters:** If a user program crashes or misbehaves, it cannot corrupt kernel memory or affect other processes.

**Code example:** See `process.c:user_entry()` which uses `sret` to drop from supervisor to user mode.

---

### 2. Virtual Memory

**Principle:** Each process should have its own isolated address space, creating the illusion that it owns the entire machine.

**Implementation:**
- **SV32 Paging:** Two-level page tables (4KB pages)
- **Address Translation:** Hardware MMU translates virtual → physical addresses
- **Per-process page tables:** Each process has a unique view of memory

**Identity Mapping for Kernel:**
```
Virtual Address = Physical Address (for kernel pages)
```
**Why?** Simplifies kernel code since kernel doesn't need to think about address translation. Trade-off: Less flexible than higher-half kernels, but much simpler for a minimal OS.

**Code:** `memory.c:map_page()` implements the two-level page table walk.

---

### 3. System Calls

**Principle:** User programs need a controlled way to request kernel services without breaking privilege isolation.

**Implementation:**
- **ECALL instruction:** RISC-V instruction that traps to supervisor mode
- **Trap Handler:** `kernel_entry()` saves all registers, dispatches to handler
- **Calling Convention:** System call number in `a3`, arguments in `a0-a2`, return value in `a0`

**System Call Set:**
```
SYS_PUTCHAR  (1) - Write one character
SYS_GETCHAR  (2) - Read one character
SYS_READFILE (4) - Read entire file
SYS_WRITEFILE(5) - Write entire file
SYS_EXIT     (3) - Terminate process
```

**Why?** This minimal set is sufficient for a shell and demonstrates the concept. Real OS would have dozens (Linux has 300+).

**Code:** `trap.c:handle_syscall()` implements the dispatch logic.

---

### 4. Process Scheduling

**Principle:** The OS must share CPU time among multiple processes to create the illusion of parallelism.

**Implementation:**
- **Cooperative Multitasking:** Processes voluntarily yield CPU via `yield()`
- **Round-Robin Scheduling:** Simple fairness - each process gets a turn
- **Context Switching:** Save/restore callee-saved registers (s0-s11, ra)

**Cooperative vs Preemptive:**

| Approach | Pros | Cons |
|----------|------|------|
| **Cooperative** (this kernel) | Simple, no timer needed | Misbehaving process can hang system |
| **Preemptive** (most OS) | Fair, responsive | Complex, requires timer interrupts |

**Why cooperative?** Minimizes complexity for educational purposes. The kernel focuses on demonstrating scheduling mechanics without the complexity of interrupt handling.

**Code:** `process.c:yield()` implements the scheduler, `switch_context()` does the low-level register swap.

---

### 5. I/O and Device Drivers

**Principle:** Hardware devices have diverse interfaces; the OS must abstract them into a uniform API.

**Implementation:**
- **Memory-Mapped I/O (MMIO):** Device registers appear at physical address `0x10001000`
- **VirtIO Protocol:** Standardized interface for virtual devices
- **Virtqueues:** Ring buffers for asynchronous device communication

**Synchronous I/O:**
```c
read_write_disk(...);
while (virtq_is_busy(vq))  // Busy-wait until complete
    ;
```

**Why?** Simple and predictable. Trade-off: Wastes CPU cycles, but acceptable for a minimal kernel. Production OS would use interrupts and schedule other work during I/O.

**Code:** `virtio.c:read_write_disk()` shows the full virtqueue descriptor chain setup.

---

### 6. File System

**Principle:** Storage devices work with fixed-size blocks; the OS must provide a file abstraction.

**Implementation:**
- **TAR Format:** Industry-standard archive format
- **In-Memory Cache:** Entire disk loaded into RAM for simplicity
- **Write-Through:** Changes immediately flushed to disk

**Why TAR?**

Pros:
-  Simple: No complex data structures (inodes, bitmaps)
-  Portable: Standard format, easy to create (`tar cf`)
-  Readable: ASCII metadata, easy to debug

Cons:
-  Limited: Fixed file count, no directories

**Alternative approaches:**
- **FAT:** More features, but complex
- **Custom:** Total control, but reinventing the wheel

**Code:** `fs.c:fs_init()` parses TAR headers, `fs_flush()` rebuilds them.

---

## Architecture & Design Decisions

### Boot Sequence

```
1. QEMU loads kernel.elf at 0x80200000
2. boot() sets up stack pointer
3. kernel_main() initializes subsystems:
   - Clear BSS (uninitialized data)
   - Set trap vector (stvec register)
   - Initialize VirtIO block device
   - Load filesystem from disk
   - Create idle process
   - Create shell process
4. yield() switches to first user process
```

**Why load at 0x80200000?**

This is the standard RISC-V kernel load address. The lower address (0x80000000) is reserved for OpenSBI (machine-mode firmware). This convention ensures compatibility across RISC-V platforms.

---

### Memory Layout

```
Physical Memory:
0x80000000 ─────────┐
           OpenSBI  │ Machine-mode firmware
0x80200000 ─────────┤
           .text    │ Kernel code
           .rodata  │ Read-only data
           .data    │ Initialized data
           .bss     │ Uninitialized data
           Stack    │ 128 KB kernel stack
__free_ram ─────────┤
           Heap     │ Dynamic allocations (pages)
           Page tbls│ Process page tables
           Process  │ Process stacks & memory
__free_ram_end ─────┘

Virtual Memory (per-process):
0x00000000 ─────────┐
           Unmapped │ Causes page fault if accessed
0x01000000 ─────────┤ USER_BASE
           User pgm │ Application code & data
0x80000000 ─────────┤
           Kernel   │ Identity-mapped
0x10001000 ─────────┤
           VirtIO   │ Device registers
```

**Single Memory Pool:**

Instead of separate allocators for different needs, `alloc_pages()` uses a bump allocator:
```c
static paddr_t next_paddr = (paddr_t) __free_ram;
next_paddr += n * PAGE_SIZE;
```

**Trade-offs:**
Pros:
-  Simple: 10 lines of code
-  Fast: O(1) allocation

Cons:
-  No deallocation: Memory never freed (acceptable for minimal OS)
-  Fragmentation: Could waste space (not an issue with 4KB pages)

---

### Context Switching Mechanics

**The Problem:** How do we save one process's state and restore another's?

**RISC-V Register Convention:**
- **Caller-saved (t0-t6, a0-a7):** Calling function saves if needed
- **Callee-saved (s0-s11, ra):** Called function must preserve

**Our Solution:**
```
switch_context(prev_sp, next_sp):
  1. Save s0-s11, ra to current stack
  2. Save current stack pointer to *prev_sp
  3. Load new stack pointer from *next_sp
  4. Restore s0-s11, ra from new stack
  5. Return (jumps to restored ra)
```

**Why only callee-saved?** The C compiler guarantees that caller-saved registers are already saved across function calls. We only need to preserve what the compiler expects.

**First Process Bootstrap:**

When a process is first created, its stack is pre-populated with:
```
sp → [user_entry function address]  ← Will be loaded into ra
     [0] s0
     [0] s1
     ...
     [0] s11
```

On first context switch, `ret` jumps to `user_entry()`, which executes `sret` to enter user mode.

---

### Trap Handling

**The Flow:**

```
User program executes: ecall
           ↓
Hardware atomically:
  - Sets sepc = PC (program counter)
  - Sets scause = 8 (ECALL exception)
  - Jumps to stvec (kernel_entry)
           ↓
kernel_entry (assembly):
  1. Swap sp with sscratch (get kernel stack)
  2. Save ALL 31 registers to trap frame
  3. Call handle_trap(trap_frame*)
           ↓
handle_trap (C):
  - Read scause to determine trap type
  - Call handle_syscall()
  - Increment sepc by 4 (skip past ecall)
           ↓
kernel_entry (assembly):
  1. Restore all 31 registers
  2. Execute sret (returns to user mode)
```

**sscratch Register:**

RISC-V provides `sscratch` as a scratch register. We use it to store the kernel stack pointer:
- User mode: sscratch = kernel stack
- Kernel mode: sscratch = user stack (swapped)

This enables atomic stack switching in one instruction (`csrrw`).

---

### SBI (Supervisor Binary Interface)

**The Problem:** How does the kernel communicate with hardware (e.g., serial console) without implementing every driver?

**The Solution:** OpenSBI provides machine-mode services that supervisor-mode OS can call:

```c
sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
```

**Register Mapping:**
- a0-a5: Arguments
- a6: Function ID (fid)
- a7: Extension ID (eid)
- Execute `ecall` to trap to machine mode

**Design Decision:** Use SBI instead of raw UART driver.

**Why?**
Pros:
- Portable: Works on any RISC-V platform
- Simple: No hardware-specific code

Cons
- Limited: Only basic console I/O available

---

## Module Guide

### memory.c/h - Memory Management

**Responsibilities:**
- Physical page allocation
- Virtual memory page table construction

**Key Function:** `map_page(table1, vaddr, paddr, flags)`

**How it works:**
1. Extract VPN[1] from virtual address (bits 31:22)
2. If no level-0 page table exists, allocate one
3. Extract VPN[0] from virtual address (bits 21:12)
4. Create PTE (Page Table Entry) with physical page number and flags

**SV32 Page Table Entry Format:**
```
31        10 9  8 7 6 5 4 3 2 1 0
[   PPN   ]  [RSW] [D A G U X W R V]

V = Valid
R/W/X = Read/Write/Execute permissions
U = User mode accessible
```

---

### virtio.c/h - Block Device Driver

**Responsibilities:**
- Initialize VirtIO block device
- Read/write 512-byte sectors

**VirtIO Architecture:**
```
Kernel                  Device
  ↓                       ↑
Virtqueue (shared memory)
  ├─ Descriptor Table (what to transfer)
  ├─ Available Ring (kernel → device)
  └─ Used Ring (device → kernel)
```

**Three-Descriptor Chain for Disk I/O:**
1. **Header:** Request type (read/write) and sector number
2. **Data:** 512-byte buffer (device-readable or writable)
3. **Status:** Result code (device writes here)

**Why three descriptors?** VirtIO protocol specification requires this structure for block devices. Separating metadata from data enables efficient DMA.

---

### fs.c/h - File System

**Responsibilities:**
- Parse TAR archive format
- Maintain in-memory file cache
- Write files back to disk

**TAR Header Structure (POSIX ustar):**
```
Offset  Size  Field
0       100   Filename
100     8     File mode (octal)
...
124     12    File size (octal)
...
257     6     Magic ("ustar")
500     12    Padding
512+    N     File data
```

TAR uses octal strings for numeric fields (size, mode). This is a Unix tradition dating back to when octal was preferred for file permissions (rwxrwxrwx = 3 bits × 3 groups = octal).

Our `oct2int()` converts: `"00000144"` → 100 (decimal)

---

### process.c/h - Process Management

**Responsibilities:**
- Create isolated processes with virtual memory
- Implement round-robin scheduler
- Perform context switches

**Process Control Block (struct process):**
```c
struct process {
    int pid;              // Process ID
    int state;            // UNUSED/RUNNABLE/EXITED
    vaddr_t sp;           // Saved stack pointer
    uint32_t *page_table; // Root page table
    uint8_t stack[8192];  // Kernel stack
};
```

**Why 8KB stack?**
- Sufficient for kernel operations (syscalls don't use much stack)
- Power of 2 for alignment
- Trade-off: Smaller saves memory, larger prevents overflow

**Idle Process:**

PID 0 is special - it has NULL image (no user code). When no processes are runnable, the scheduler switches to idle. This prevents the scheduler from panicking if all processes block.

---

### trap.c/h - Trap & Syscall Handling

**Responsibilities:**
- Save/restore all registers on trap entry/exit
- Dispatch system calls
- Handle exceptions

**Trap Frame Layout:**
```c
struct trap_frame {
    uint32_t ra, gp, tp;   // Special registers
    uint32_t t0-t6;        // Temporary registers
    uint32_t a0-a7;        // Argument/return registers
    uint32_t s0-s11;       // Saved registers
    uint32_t sp;           // Stack pointer
};
```

We must preserve user state across traps. Even though C functions only guarantee callee-saved registers, syscalls can yield() to other processes, so we need complete state.

---

### kernel.c - Boot & Initialization

**Responsibilities:**
- Boot entry point (`boot()`)
- BSS clearing
- Subsystem initialization
- SBI interface

**BSS Clearing:**
```c
memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
```

**Why?** C standard requires uninitialized global variables to be zero. In a freestanding environment, we must do this manually. The linker provides `__bss` and `__bss_end` symbols.

---

## Building and Running

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install clang lld qemu-system-riscv32

# macOS
brew install llvm qemu
```

### Build and Run

```bash
./run.sh
```

**What it does:**
1. Compiles shell (user program)
2. Converts shell to raw binary and embeds it in kernel
3. Compiles kernel with all modules
4. Creates TAR disk image
5. Launches QEMU with kernel and disk

**Exit QEMU:** Press `Ctrl-A` then `X`

### File Structure

```
.
├── kernel.c/h        - Boot and initialization
├── common.c/h        - Standard library (memcpy, printf, etc.)
├── memory.c/h        - Memory management
├── virtio.c/h        - Block device driver
├── fs.c/h            - File system
├── process.c/h       - Process scheduling
├── trap.c/h          - Trap and syscall handling
├── user.c/h          - User library (syscall wrappers)
├── shell.c           - Shell application
├── kernel.ld         - Kernel linker script
├── user.ld           - User program linker script
└── run.sh            - Build script
```

---

## Learning Path

### Recommended Order

Progression:

#### 1. **Start with the Tutorial**

Work through the original tutorial first to understand the concepts:
- [Operating System in 1000 Lines Tutorial](https://1000os.seiya.me/en/)

The tutorial provides step-by-step explanations and builds the OS incrementally.

#### 2. **Beginner Topics / Basics**

1. **`common.c`** - See how `printf()` is implemented from scratch
2. **`kernel.c:kernel_main()`** - Understand initialization sequence
3. **`shell.c`** - Study how user programs interact with kernel
4. **`user.c:syscall()`** - Learn the syscall interface

#### 3. **Intermediate Topics - Subsystems**

5. **`process.c:create_process()`** - Learn how processes are created
6. **`trap.c:handle_syscall()`** - See user-kernel boundary
7. **`memory.c:map_page()`** - Understand page tables
8. **`fs.c:fs_init()`** - Learn TAR format parsing

#### 4. **Advanced Topics - Low level details**

9. **`process.c:switch_context()`** - Study assembly context switching
10. **`trap.c:kernel_entry()`** - Deep dive into trap handling
11. **`virtio.c:read_write_disk()`** - Understand device communication
12. **Linker scripts (`kernel.ld`)** - Learn memory layout control

---

## Extension Ideas

### Easy
- [ ] Add `ls` command to list all files
- [ ] Implement `%c` (character) format specifier in `printf()`
- [ ] Add a `cat <filename>` command to display file contents
- [ ] Create a simple `echo` command

### Medium
- [ ] Implement preemptive scheduling with timer interrupts
- [ ] Add `fork()` syscall to create child processes
- [ ] Implement basic dynamic memory allocation (`malloc()`/`free()`)
- [ ] Support more files in filesystem (increase FILES_MAX)
- [ ] Add file creation/deletion syscalls

### Hard
- [ ] Add keyboard input via UART driver (remove SBI dependency)
- [ ] Implement proper resource cleanup on process exit (free pages)
- [ ] Add inter-process communication (pipes or message passing)
- [ ] Implement copy-on-write for efficient `fork()`
- [ ] Port to RISC-V 64-bit (RV64)

### Very Hard
- [ ] Implement a hierarchical filesystem with directories
- [ ] Add network support via VirtIO-net device
- [ ] Implement demand paging (lazy allocation)
- [ ] Multi-core support with SMP
- [ ] Add ELF loader to run arbitrary executables

---

## Design Philosophy

This kernel embodies several pedagogical principles:

### 1. Simplicity Over Features

We intentionally omit features that real OS have:
- No memory deallocation -> Simpler allocator
- Cooperative scheduling -> No timer complexity
- In-memory filesystem -> No caching logic
- Limited syscalls -> Smaller attack surface

**Goal:** Demonstrate *concepts*, not build a production OS.

### 2. Readability Over Performance

Examples of choosing clarity:
- Busy-wait for I/O instead of interrupts
- Linear search for file lookup
- Memset entire pages on allocation

**Goal:** Code you can understand in one sitting.

### 3. Standard Compliance Where Possible

We use:
- Standard TAR format (not custom)
- VirtIO protocol (not ad-hoc device interface)
- RISC-V calling convention (standard ABI)
- SBI interface (portable)

**Goal:** Real-world relevance, not toy examples.

---

## Common Questions

**Q: Why RISC-V and not x86?**

A: RISC-V is simpler:
- No legacy baggage (A20 gate, real mode, etc.)
- Clean privilege model (M/S/U modes)
- Well-documented and open
- Popular in education

**Q: Why cooperative scheduling?**

A: Preemptive scheduling requires:
- Timer interrupt handling
- More complex context switching
- Interrupt masking logic

Cooperative is 90% simpler for learning.

**Q: Why no memory deallocation?**

A: Implementing a proper allocator (buddy system, slab, etc.) would double the code size. For a minimal OS, a bump allocator suffices.

**Q: Is this secure?**

A: **No!** This is educational code. Known issues:
- No bounds checking in `strcpy()`
- No resource limits
- Cooperative scheduling allows denial of service
- No ASLR or other mitigations

**Q: Can I use this for real projects?**

A: Not recommended. Use established OS like Linux, or RTOS like FreeRTOS or Zephyr. This kernel is for *learning*, not production.

**Q: How does this compare to xv6?**

A: Both are teaching operating systems, but with different goals:
- **This kernel:** Extreme simplicity (~1000 lines), RISC-V only
- **xv6:** More complete (~6000 lines), includes shell, filesystem, more syscalls

Choose this for quick learning, xv6 for deeper study.

---

## References

### Original Work

- **[Operating System in 1000 Lines](https://1000os.seiya.me/en/)** - Complete tutorial by Seiya Nuta
- **[GitHub Repository](https://github.com/nuta/operating-system-in-1000-lines)** - Original source code
- **Author:** [Seiya Nuta](https://github.com/nuta)

### RISC-V Specifications

- [RISC-V Privileged Specification](https://riscv.org/technical/specifications/) - Defines S-mode, page tables, CSRs
- [RISC-V SBI Specification](https://github.com/riscv-non-isa/riscv-sbi-doc) - Supervisor Binary Interface
- [RISC-V Instruction Set Manual](https://riscv.org/technical/specifications/) - Complete ISA documentation

### VirtIO

- [VirtIO Specification v1.1](https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html) - Device interface standard
- [VirtIO Block Device](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-2630002) - Block device specifics

### Operating Systems Textbooks

- [*Operating Systems: Three Easy Pieces*](https://pages.cs.wisc.edu/~remzi/OSTEP/) - Excellent free textbook (Remzi & Andrea Arpaci-Dusseau)
- [*xv6: A simple Unix-like teaching operating system*](https://pdos.csail.mit.edu/6.828/2023/xv6.html) - MIT's RISC-V OS with book
- *Operating System Concepts* by Silberschatz, Galvin, Gagne - The "dinosaur book"
- *Modern Operating Systems* by Andrew Tanenbaum - Comprehensive overview

### RISC-V Resources

- [*The RISC-V Reader*](http://riscvbook.com/) - Accessible introduction
- *Computer Organization and Design RISC-V Edition* by Patterson & Hennessy - The classic textbook
- [RISC-V Software Ecosystem](https://riscv.org/software/) - Tools and resources

### Related Projects

- [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) - MIT's teaching OS
- [Writing an OS in Rust](https://os.phil-opp.com/) - OS development blog series
- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development resource
- [Linux Kernel](https://github.com/torvalds/linux) - For comparison with production OS

---

## Contributing

This is a refactored educational project. If you find issues or have improvements:

1. **Documentation:** Feel free to improve explanations
2. **Code clarity:** Suggest clearer variable names or comments
3. **Bug fixes:** Report or fix actual bugs

Keep in mind the goal is educational simplicity, not feature completeness.

---

## License

Based on the original work by Seiya Nuta. This refactored version maintains the educational spirit of the original project.

For educational purposes. Feel free to use, modify, and learn from it.

---

## Acknowledgments

- **Seiya Nuta** - For creating the original "Operating System in 1000 Lines" and excellent tutorial
- MIT's xv6 team - For pioneering teaching operating systems
- The RISC-V Foundation - For creating an open, learnable ISA
