#include "filesys/off_t.h"
#include "lib/kernel/hash.h"

// DRIVER: TIMOTHY
struct spage_entry {
    // location of the page if in swap, -1 if not in swap
    int swap_slot;
    void *page_ptr;
    struct file *f;
    off_t offset;
    uint32_t read_bytes;
    struct hash_elem elem;
    bool writable;
};

void spage_init(struct hash *spage_table);
void spage_add (struct hash *h, void *page_ptr, struct file *f, off_t offset,
                uint32_t read_bytes, bool writable);
struct spage_entry *spage_get_entry(struct hash *h, void *page);
void spage_set_swap_slot(struct hash *h, void *page, int swap_index);
void spage_free(void);
