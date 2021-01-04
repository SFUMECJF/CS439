#include "swap.h"
#include "devices/block.h"

// DRIVER: PREETH
void swap_init() {
    swap_block = block_get_role(BLOCK_SWAP);
    size = block_size(swap_block) / 8;
    lock_init(&swap_lock);
    bmap = bitmap_create(size);
}

// DRIVER: PREETH
block_sector_t swap_index_to_sector(int index) {
    return index * 8;
}

// DRIVER: TIMOTHY
// writes it from frame into a free swap slot, and returns the index of it
int swap_add(void *frame) {
    char *f = (char *) frame;
    int slot = swap_get_slot();
    if(slot == -1){
        return -1;
    }
    block_sector_t sector = swap_index_to_sector(slot);
    int i;
    for(i = 0; i < 8; sector++, f += BLOCK_SECTOR_SIZE, i++){
        block_write(swap_block, sector, f);
    }
    return slot;
}
 
// DRIVER: TIMOTHY
// returns index of a free swap slot, or -1 if swap is full, and updates bitmap
int swap_get_slot() {
    int slot = -1;
    lock_acquire(&swap_lock);
    size_t i;
    for(i = 0; i < size && slot == -1; i++) {
        slot = bitmap_test(bmap, i) ? -1 : (bitmap_mark(bmap, i), i);
    }
    lock_release(&swap_lock);
    return slot;
}

// DRIVER: JUSTIN
// copies one page from swap slot to frame, then frees that swap slot
void swap_remove(void *frame, int slot) {
    char *f = (char *) frame;
    block_sector_t sector = swap_index_to_sector(slot);
    size_t i;
    for(i = 0; i < 8; sector++, f += BLOCK_SECTOR_SIZE, i++){
        block_read(swap_block, sector, f);
    }
    swap_free(slot);
}

// DRIVER: JUSTIN
// frees the given swap slot and updates bitmap
void swap_free(int slot) {
    lock_acquire(&swap_lock);
    bitmap_reset(bmap, slot);
    lock_release(&swap_lock);
}
