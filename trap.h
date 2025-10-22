#pragma once
#include "common.h"
#include "kernel.h"

/**
 * Assembly trap entry point.
 * Saves all registers to the trap frame, calls handle_trap, then restores
 * registers and returns to user mode via sret.
 */
void kernel_entry(void);

/**
 * High-level trap handler.
 * Dispatches to the appropriate handler based on trap cause.
 *
 * @param f - Pointer to the trap frame containing saved register state
 */
void handle_trap(struct trap_frame *f);