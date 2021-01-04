#include "threads/palloc.h"
#include "frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"


// DRIVER: JUSTIN
void frame_init(){
    // init fields
    clock_index = 0;
    num_used_frames = 0;
    max_frames = DEFAULT_MAX_FRAMES;
    // allocate frame_array
    frame_array = malloc(sizeof(struct frame_elem) * max_frames);
    if(frame_array == NULL){
        PANIC("out OF MEMORUY");
    }
    // allocate each frame pointed to by a frame_elem in frame_array
    unsigned i;
    for(i = 0; i < max_frames; i++){
        // set up each frame elem
        struct frame_elem *curr = &frame_array[i];
        curr->frame_ptr = palloc_get_page(PAL_USER);
        curr->page_ptr = NULL;
        lock_init(&curr->frame_lock);
    }
    lock_init(&frame_table_lock);
}

// DRIVER: JUSTIN
// returns the pointer to a free frame elem
struct frame_elem *frame_allocate(void *page_ptr) {
    lock_acquire(&frame_table_lock);
    // check if need to evict
    if(num_used_frames >= max_frames){
        frame_evict();
    }

    // find free frame
    struct frame_elem *free_frame = frame_array;
    int found = 0;
    while(!found){
        if(free_frame->page_ptr == NULL){
            found = 1;
        } else {
            free_frame++;
        }
    }
    
    struct thread *cur = thread_current();
    // update frame table entry
    free_frame->page_ptr = page_ptr;
    free_frame->t_ptr = cur;

    num_used_frames++;
    lock_release(&frame_table_lock);
    return free_frame;
}

// DRIVER: TIMOTHY
// evicts a frame using non-enhanced clock
void frame_evict() {
    struct thread *cur = thread_current();
    // find frame to evict
    int found = 0;
    // loop from clock_index in frame_array
    for(; !found; clock_index = (clock_index + 1) % max_frames){
        struct frame_elem *f = &frame_array[clock_index];
        if(f->t_ptr->pinning) {
            continue;
        }
        // if frame is accessed, set to unaccessed and move on, else found it
        if(pagedir_is_accessed (f->t_ptr->pagedir, f->page_ptr)){
            pagedir_set_accessed (f->t_ptr->pagedir, f->page_ptr, 0);
        } else {
            found = 1;
        }
    }
    struct frame_elem *to_evict = &frame_array[clock_index];
    lock_acquire(&to_evict->frame_lock);
    // if dirty, move to swap
    if(pagedir_is_dirty(to_evict->t_ptr->pagedir, to_evict->page_ptr)){
        // put in swap
        to_evict->t_ptr->pinning = true;
        int swap_index = swap_add(to_evict->page_ptr);
        to_evict->t_ptr->pinning = false;
        if(swap_index == -1){
            PANIC("SWAP IS FULL");
        }
        // update spage table to indicate what swap slot its now in
        spage_set_swap_slot(&to_evict->t_ptr->spage_table, to_evict->page_ptr,
                             swap_index);
    }
    // update page table
    pagedir_clear_page(to_evict->t_ptr->pagedir, to_evict->page_ptr);
    // update frame table so that this frame is now free
    to_evict->page_ptr = NULL;
    num_used_frames--;
    clock_index = (clock_index + 1) % max_frames;
    lock_release(&to_evict->frame_lock);
}

// DRIVER: PREETH
// sets the frame table entries belonging to this thread to be unused
void frame_free(){
    lock_acquire(&frame_table_lock);
    int i;
    for(i = 0; i < max_frames; i++){
        struct frame_elem *elem = &frame_array[i];
        lock_acquire(&elem->frame_lock);
        if(elem->t_ptr == thread_current()){
            elem->page_ptr = NULL;
            elem->t_ptr = NULL;
        }
        lock_release(&elem->frame_lock);
    }
    lock_release(&frame_table_lock);
}
