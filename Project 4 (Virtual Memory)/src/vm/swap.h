#include "lib/kernel/bitmap.h"
#include "threads/synch.h"
#include "devices/block.h"

// DRIVER: PREETH
struct bitmap *bmap;
size_t size;
struct lock swap_lock;
struct block *swap_block;

void swap_init(void);
int swap_add(void *frame);
void swap_remove(void *frame, int slot);
void swap_free(int slot);
int swap_get_slot(void);
block_sector_t swap_index_to_sector(int index);
