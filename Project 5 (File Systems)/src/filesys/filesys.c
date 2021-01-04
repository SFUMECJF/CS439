#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  struct dir *parent_dir;
  char *file_name;
  if (!filesys_check_path(name, &parent_dir, &file_name)) return false;

  block_sector_t inode_sector = 0;
  bool success = (parent_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (parent_dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  struct inode *node = inode_open(inode_sector);
  inode_set_parent_inode(node, inode_get_inumber(dir_get_inode(parent_dir)));
  inode_close(node);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode;
  if (!filesys_parse_path(name, &inode, NULL)) return NULL;

  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct inode *inode;
  char *file_name;
  if (!filesys_parse_path(name, &inode, &file_name)) return false;

  struct dir *dir = dir_open(inode_get_parent_inode(inode));
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

// Parses the path name, returning true if the file/dir exists,
// storing the inode in output
bool filesys_parse_path (const char *path, struct inode **output, char **name) {
  int len = strlen(path) + 1;
  char *_path = malloc(len * sizeof(char));
  strlcpy(_path, path, len);

  if (*_path == '\0') return false;

  struct dir *curr_dir;
  // check if absolute or relative path
  if (*_path == '/') {
    curr_dir = dir_open_root();
  } else {
    curr_dir = thread_current()->cwd;
    if (!curr_dir) curr_dir = dir_open_root();
  }

  char *saveptr;
  char *token = strtok_r(_path, "/", &saveptr);
  struct inode *curr_inode = dir_get_inode(curr_dir);
  while (token != NULL) {
    if (name) *name = token;
    if (!dir_lookup(curr_dir, token, &curr_inode)) return false;
    if (!inode_isdir(curr_inode)) {
      *output = curr_inode;
      return true;
    }
    // else, keep going deeper
    curr_dir = dir_open(curr_inode);
    token = strtok_r(NULL, "/", &saveptr);
  }

  // this means the end is a directory
  *output = curr_inode;
  return true;
}

// Similar to filesys_parse_path, but instead checks if this path is viable
// to create a new file/directory, and returns the parent in output
bool filesys_check_path (const char *path, struct dir **output, char **name) {
  int len = strlen(path) + 1;
  char *_path = malloc(len * sizeof(char));
  strlcpy(_path, path, len);
  if (*_path == '\0') return false;

  struct dir *curr_dir;
  // check if absolute or relative path
  if (*_path == '/') {
    curr_dir = dir_open_root();
  } else {
    curr_dir = thread_current()->cwd;
    if (!curr_dir) curr_dir = dir_open_root();
  }

  char *saveptr, *next_token;
  char *token = strtok_r(_path, "/", &saveptr);
  struct inode *curr_inode;
  while (token != NULL) {
    next_token = strtok_r(NULL, "/", &saveptr);
    if (!dir_lookup(curr_dir, token, &curr_inode)) {
      if (next_token) {
        // intermediate directory doesnt exist
        return false;
      } else {
        // final location doesnt exist => available
        *output = curr_dir;
        *name = token;
        return true;
      }
    }
    curr_dir = dir_open(curr_inode);
    token = next_token;
  }

  // this means the final location exists => not available
  return false;
}
