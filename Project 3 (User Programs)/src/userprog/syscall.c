#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
void write(struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  int syscall = *((int *) f->esp);
  switch(syscall){
    case SYS_WRITE: write(f);
  }
  thread_exit ();
}

void
write(struct intr_frame *f){
  // hex_dump(f->esp, f->esp, PHYS_BASE - (f->esp), 69);

  int file = *((int *) (f->esp + 4));
  void* buffer = (f->esp + 8);
  int size = *((int *) (f->esp + 12));
  
  printf("buffer: %p\n", buffer);

  // struct thread *t = thread_current();
  // pagedir_activate (t->pagedir);
  
  // buffer = pagedir_get_page (t->pagedir, buffer);
  // printf("buffer after get page: %p\n", buffer);

  // hex_dump(buffer, buffer, 10, 69);

  // buffer = vtop(buffer);
  // printf("file: %d\n", *((int *) (f->esp + 4)));
  // printf("buffer after vtop: %p\n", buffer);
  // printf("size: %d\n", *((int *) (f->esp + 12)));
  
  if(file == 1){
    putbuf((char *) buffer, size);
    f->eax = size;
  }
}
