#include "threads/synch.h"

#define DEFAULT_MAX_FRAMES 367

// DRIVER: Justin
struct frame_elem {
    void *frame_ptr;
    void *page_ptr;
    struct lock frame_lock;
    // thread whose page is in this frame
    struct thread *t_ptr;
};

struct lock frame_table_lock;
size_t clock_index;
size_t num_used_frames;
size_t max_frames;
struct frame_elem *frame_array;

void frame_init(void);
struct frame_elem *frame_allocate(void *page_ptr);
void frame_evict(void);
void frame_free(void);
