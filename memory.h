#pragma once
#include "common.h"
#include "kernel.h"

/**
 * Allocates n contiguous physical pages and returns the physical address.
 * Pages are zeroed before returning.
 */
paddr_t alloc_pages(uint32_t n);

/**
 * Maps a virtual address to a physical address in the given page table.
 * Creates intermediate page table entries as needed.
 *
 * @param table1 - Pointer to the level-1 page table
 * @param vaddr - Virtual address to map (must be page-aligned)
 * @param paddr - Physical address to map to (must be page-aligned)
 * @param flags - Page table entry flags (PAGE_R, PAGE_W, PAGE_X, PAGE_U)
 */
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);
