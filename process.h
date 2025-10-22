#pragma once
#include "common.h"
#include "kernel.h"

/* Global process state */
extern struct process procs[PROCS_MAX];
extern struct process *current_proc;
extern struct process *idle_proc;

/**
 * Creates a new process with the given user image.
 * Sets up page tables, maps kernel and user pages, and initializes the stack.
 *
 * @param image - Pointer to the user program binary
 * @param image_size - Size of the user program in bytes
 * @return Pointer to the created process
 */
struct process *create_process(const void *image, size_t image_size);

/**
 * Performs cooperative multitasking by switching to the next runnable process.
 * Uses round-robin scheduling.
 */
void yield(void);

/**
 * Low-level context switch between two processes.
 * Saves callee-saved registers and switches stack pointers.
 *
 * @param prev_sp - Pointer to previous process's stack pointer
 * @param next_sp - Pointer to next process's stack pointer
 */
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);