#pragma once
#include "common.h"
#include "kernel.h"

/**
 * Initializes the filesystem by loading all files from disk into memory.
 * Reads the TAR-formatted disk and populates the file table.
 */
void fs_init(void);

/**
 * Writes all in-memory files back to disk.
 * Reconstructs the TAR format and writes to the virtio-blk device.
 */
void fs_flush(void);

/**
 * Looks up a file by name in the file table.
 *
 * @param filename - Name of the file to find
 * @return Pointer to the file structure, or NULL if not found
 */
struct file *fs_lookup(const char *filename);