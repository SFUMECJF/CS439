#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/thread.h"

unsigned spage_hash_func (const struct hash_elem *e, void *aux);
void spage_free_entries(struct hash_elem *e, void* aux);

bool spage_less_func (const struct hash_elem *a,
                             const struct hash_elem *b,
                             void *aux);

// DRIVER: PREETH
void spage_init(struct hash *spage_table){
    hash_init(spage_table, &spage_hash_func, &spage_less_func, NULL);
}

// DRIVER: PREETH
unsigned spage_hash_func (const struct hash_elem *e, void *aux) {
    return (unsigned) hash_entry(e, struct spage_entry, elem)->page_ptr;
}

// DRIVER: PREETH
bool spage_less_func (const struct hash_elem *a,
                             const struct hash_elem *b,
                             void *aux) {
    return hash_entry(a, struct spage_entry, elem)->page_ptr < 
           hash_entry(b, struct spage_entry, elem)->page_ptr;
}

// DRIVER: TIMOTHY
void spage_add (struct hash *h, void *page_ptr, struct file *f, off_t offset,
                uint32_t read_bytes, bool writable) {
    struct spage_entry *e = malloc(sizeof(struct spage_entry));
    e->swap_slot = -1;
    e->page_ptr = page_ptr;
    e->f = f;
    e->offset = offset;
    e->read_bytes = read_bytes;
    e->writable = writable;
    hash_insert(h, &e->elem);
}

// DRIVER: TIMOTHY
// returns spage entry of the given page if in table, else null
struct spage_entry *spage_get_entry(struct hash *h, void *page) {
    struct spage_entry e;
    e.page_ptr = page;
    struct hash_elem *elem = hash_find(h, &e.elem);
    return elem ? hash_entry(elem, struct spage_entry, elem) : NULL;
}

// DRIVER: TIMOTHY
void spage_set_swap_slot(struct hash *h, void *page, int swap_index) {
    struct spage_entry e;
    e.page_ptr = page;
    struct hash_elem *elem = hash_find(h, &e.elem);
    hash_entry(elem, struct spage_entry, elem)->swap_slot = swap_index;
}

// DRIVER: TIMOTHY
// frees entries of the hash table, as well as the table itself
void spage_free() {
    // free hash table
    hash_destroy(&thread_current()->spage_table, &spage_free_entries);
}

// DRIVER: TIMOTHY
// frees entries of hash table and sets swap slots to be unused
void spage_free_entries(struct hash_elem *e, void* aux) {
    struct spage_entry *entry = hash_entry(e, struct spage_entry, elem);
    // if in swap, free that swap slot
    if(entry->swap_slot != -1){
        swap_free(entry->swap_slot);
    }
    // free entry
    free(entry);
}
