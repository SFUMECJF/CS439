#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "lib/user/syscall.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "userprog/process.h"
#include "threads/palloc.h"

static void syscall_handler (struct intr_frame *);
int write_helper(int fd, const void *buffer, unsigned size);
int open_helper(char* name);
int allocate_fd(void);
void *validate_pointer(const void *ptr);
void validate_buffer(const void *buffer, unsigned size);
bool create_helper(const char *file, unsigned initial_size);
bool remove_helper(const char *file);
int filesize_helper(int fd);
bool is_file_open(int fd);
int read_helper(int fd, void *buffer, unsigned size);
void seek_helper(int fd, unsigned position);
unsigned tell_helper(int fd);
int exec_helper(const char *cmd_line);
void close_helper(int fd);
bool chdir_helper (const char *dir);
bool mkdir_helper (const char *dir);
bool readdir_helper (int fd, char *name);
bool isdir_helper (int fd);
int inumber_helper (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&syscall_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  void *myEsp = f->esp;
  validate_pointer(myEsp);
  int syscall = *((int *) myEsp);
  int file;
  void *buffer;
  int size;
  
  // DRIVER: ALL, see helpers
  switch (syscall)
  {
    // 3 arguments
    case SYS_WRITE: case SYS_READ:
    validate_pointer(myEsp + 12);
    // 2 arguments
    case SYS_CREATE: case SYS_SEEK: case SYS_READDIR:
    validate_pointer(myEsp + 8);
    // 1 argument
    case SYS_EXIT: case SYS_WAIT: case SYS_OPEN: case SYS_REMOVE:
    case SYS_TELL: case SYS_EXEC: case SYS_FILESIZE: case SYS_CLOSE:
    case SYS_CHDIR: case SYS_MKDIR: case SYS_ISDIR: case SYS_INUMBER:
    validate_pointer(myEsp + 4);
  }

  switch(syscall){
    case SYS_WRITE:
      file = *(int *) (myEsp + 4);
      buffer = *(void **) (myEsp + 8);
      size = *(int *) (myEsp + 12); 
      f->eax = (int) write_helper(file, buffer, size);
      break;
    case SYS_EXIT:
      thread_current ()->exit_status = *(int *) (myEsp + 4);
      thread_exit ();
      break;
    case SYS_WAIT:
      f->eax = process_wait (*(int *) (myEsp + 4));
      break;
    case SYS_HALT:
      shutdown_power_off();
      break;
    case SYS_OPEN:
      f->eax = open_helper(*(char**)(myEsp + 4));
      break;
    case SYS_CREATE:
      f->eax = create_helper(*(char**)(myEsp + 4), *(unsigned *) (myEsp + 8));
      break;
    case SYS_REMOVE:
      f->eax = remove_helper(*(char**)(myEsp + 4));
      break;
    case SYS_READ:
      file = *(int *) (myEsp + 4);
      buffer = *(void **) (myEsp + 8);
      size = *(int *) (myEsp + 12); 
      f->eax = read_helper(file, buffer, size);
      break;
    case SYS_SEEK:
      seek_helper(*(int *) (myEsp + 4), *(int *) (myEsp + 8));
      break;
    case SYS_TELL:
      f->eax = tell_helper(*(int *) (myEsp + 4));
      break;
    case SYS_EXEC:
      f->eax = exec_helper (*(char**)(myEsp + 4));
      break;
    case SYS_FILESIZE:
      f->eax = filesize_helper(*(int *)(myEsp + 4));
      break;
    case SYS_CLOSE:
      close_helper(*(int *)(myEsp + 4));
      break;
    case SYS_CHDIR:
      f->eax = chdir_helper(*(char**)(myEsp + 4));
      break;
    case SYS_MKDIR:
      f->eax = mkdir_helper(*(char**)(myEsp + 4));
      break;
    case SYS_READDIR:
      f->eax = readdir_helper(*(int *)(myEsp + 4), *(char**)(myEsp + 8));
      break;
    case SYS_ISDIR:
      f->eax = isdir_helper(*(int *)(myEsp + 4));
      break;
    case SYS_INUMBER:
      f->eax = inumber_helper(*(int *)(myEsp + 4));
      break;
    
  } 
}

// Changes the current working directory of the process to dir, which may be 
// relative or absolute. Returns true if successful, false on failure.
bool chdir_helper (const char *dir) {
  struct inode *new_dir;
  if (!filesys_parse_path(dir, &new_dir, NULL)) {
    return false;
  }

  dir_close(thread_current()->cwd);
  thread_current()->cwd = dir_open(new_dir);

  return true;
}

bool mkdir_helper (const char *dir) {  
  struct dir *parent_dir;
  char *name;
  if (!filesys_check_path(dir, &parent_dir, &name)) {
    return false;
  }
  
  block_sector_t inode_sector = 0;
  bool success = (parent_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (parent_dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  struct inode *node = inode_open(inode_sector);
  inode_set_parent_inode(node, inode_get_inumber(dir_get_inode(parent_dir)));
  inode_close(node);

  return success;
}

bool readdir_helper (int fd, char *name) {
  if(fd < 0 || fd > 127){
    return 0;
  }
  if (!thread_current()->dirs[fd]) return false;

  return dir_readdir(thread_current()->dirs[fd], name);
}

bool isdir_helper (int fd) {
  if(fd < 0 || fd > 127){
    return 0;
  }
  return thread_current()->dirs[fd];
}

// Returns the inode number of the inode associated with fd, which may 
// represent an ordinary file or a directory.
int inumber_helper (int fd) {
  if(fd < 3 || fd > 127){
    return -1;
  }
  if(thread_current()->files[fd]) {
    // its a file
    return inode_get_inumber(file_get_inode(thread_current()->files[fd]));
  } else if (thread_current()->dirs[fd]) {
    // its a dir
    return inode_get_inumber(dir_get_inode(thread_current()->dirs[fd]));
  }
  return -1;
}

// DRIVER: PREETH
void close_helper(int fd)
{
  if(fd < 3 || fd > 127){
    return;
  }
  lock_acquire(&syscall_lock);
  if (thread_current()->files[fd]) {
    file_close(thread_current()->files[fd]);
    thread_current()->files[fd] = NULL;
  } else if (thread_current()->dirs[fd]) {
    dir_close(thread_current()->dirs[fd]);
    thread_current()->dirs[fd] = NULL;    
  }
  thread_current()->num_files_open--;
  lock_release(&syscall_lock);
}

// DRIVER: TIMOTHY
int exec_helper(const char *cmd_line)
{
  validate_pointer(cmd_line);
  char *file_name = palloc_get_page (0);
  if (file_name == NULL) return -1;
  strlcpy (file_name, cmd_line, PGSIZE);

  int tid = process_execute (file_name);
  palloc_free_page (file_name);
  // wait for the process to be loaded
  sema_down (&thread_current ()->exec_sema);

  if (!thread_current ()->load_success) {
    return -1;
  }
  return tid;
}

// DRIVER: PREETH
unsigned tell_helper(int fd){
  lock_acquire(&syscall_lock);
  if(!is_file_open(fd)){
    lock_release(&syscall_lock);
    return -1;
  }
  int ans = file_tell(thread_current()->files[fd]);
  lock_release(&syscall_lock);
  return ans;
}

// DRIVER: JUSTIN
void seek_helper(int fd, unsigned position){
  lock_acquire(&syscall_lock);
  if(!is_file_open(fd)){
    lock_release(&syscall_lock);
    return;
  }
  file_seek(thread_current()->files[fd], position);
  lock_release(&syscall_lock);
}

// DRIVER: TIMOTHY
bool 
create_helper(const char *file, unsigned initial_size){
  validate_pointer(file);
  lock_acquire(&syscall_lock);

  bool success = filesys_create(file, initial_size);

  lock_release(&syscall_lock);
  return success;
}

// DRIVER: BRUNO
int
filesize_helper(int fd){
  lock_acquire(&syscall_lock);
  if(!is_file_open(fd)){
    lock_release(&syscall_lock);
    return -1;
  }
  int result = file_length(thread_current()->files[fd]);
  lock_release(&syscall_lock);
  return result;
}

// DRIVER: TIMOTHY
bool is_file_open(int fd){
  if(fd < 0 || fd > 127){
    return 0;
  }
  return thread_current()->files[fd];
}

// DRIVER: BRUNO
bool 
remove_helper(const char *file){
  validate_pointer(file);
  lock_acquire(&syscall_lock);
  bool success = filesys_remove(file);
  lock_release(&syscall_lock);
  return success;
}

// DRIVER: TIMOTHY
int
open_helper(char *name){
  validate_pointer(name);
  lock_acquire(&syscall_lock);
  struct thread *cur = thread_current();
  // can't open too many files
  if(cur->num_files_open >= MAX_FILES_OPEN){
    lock_release(&syscall_lock);
    return -1;
  }

  struct file *f = filesys_open(name);
  if(f == NULL){
    lock_release(&syscall_lock);
    return -1;
  }

  int fd = allocate_fd();
  struct inode *inode = file_get_inode(f);
  if (inode_isdir(inode)) {
    struct dir *dir = dir_open(inode);
    cur->dirs[fd] = dir;
    file_close(f);
  } else {
    cur->files[fd] = f;
  }

  cur->num_files_open++;
  lock_release(&syscall_lock);
  return fd;
}

// DRIVER: JUSTIN
// checks if pointer is valid
void*
validate_pointer(const void *ptr){
  if (ptr == NULL || !is_user_vaddr(ptr))
  {
    thread_exit();
  }
  void *page = pagedir_get_page(thread_current()->pagedir, ptr);
  if (page == NULL)
  {
    thread_exit();
  }
  return page;
}

// DRIVER: PREETH
// chekcs if buffer is valid
void
validate_buffer(const void *buffer, unsigned size) {
  void *page = validate_pointer(buffer);
  if (buffer + size >= page + PGSIZE)
  {
    void *end = buffer + size;
    buffer = page + PGSIZE;
    while (buffer < end)
    {
      validate_pointer(buffer);
      buffer += PGSIZE;
    }
    validate_pointer(end);
  }
}

// DRIVER: TIMOTHY
// finds the next open fd
int allocate_fd() {
  struct thread *cur = thread_current();
  while(cur->files[(cur->last_fd + 1) % MAX_FILES_OPEN] != NULL || 
        cur->dirs[cur->last_fd = (cur->last_fd + 1) % MAX_FILES_OPEN] != NULL);
  return cur->last_fd;
}

// DRIVER: BRUNO
int
write_helper(int fd, const void *buffer, unsigned size){
  validate_buffer(buffer, size);
  lock_acquire(&syscall_lock);
  if(!is_file_open(fd) || fd == 0){
    lock_release(&syscall_lock);
    return -1;
  }
  if(fd == 1){
    // write to stdout
    while (size >= 420)
    {
      putbuf(buffer, 420);
      buffer += 420;
      size -= 420;
    }
    putbuf(buffer, size);
    lock_release(&syscall_lock);
    return size;
  }
  int bytes_written = file_write(thread_current()->files[fd], buffer, size);
  lock_release(&syscall_lock);
  return bytes_written;
}

// DRUVER: BRUNO
int
read_helper(int fd, void *buffer, unsigned size){
  validate_buffer(buffer, size);
  lock_acquire(&syscall_lock);
  if(!is_file_open(fd) || fd == 1){
    lock_release(&syscall_lock);
    return -1;
  }
  if(fd == 0){
    char *buf = buffer;
    while (size--)
    {
      *buf = input_getc();
      buf++;
    }
    lock_release(&syscall_lock);
    return size;
  }
  int bytes_read = file_read(thread_current()->files[fd], buffer, size);
  lock_release(&syscall_lock);
  return bytes_read;
}
