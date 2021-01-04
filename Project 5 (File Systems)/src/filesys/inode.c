#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DATA_BLOCKS 122
#define PTRS_PER_BLOCK BLOCK_SECTOR_SIZE / sizeof(block_sector_t)


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */

struct inode_disk {
  off_t length;
  block_sector_t parent_inode;
  bool is_dir;
  block_sector_t data_blocks[NUM_DATA_BLOCKS];
  block_sector_t primary_block;
  block_sector_t secondary_block;
  unsigned magic;
};

bool inode_extend(struct inode_disk *disk_inode, 
              block_sector_t sector, 
              off_t length);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

struct inode *inode_get_parent_inode(struct inode *node) {
  return inode_open(node->data.parent_inode);
}

void inode_set_parent_inode(struct inode *node, block_sector_t p) {
  node->data.parent_inode = p;
  block_write(fs_device, node->sector, &node->data);
}

bool inode_isdir(struct inode *node) {
  return node->data.is_dir;
}

void inode_set_isdir(struct inode *node, bool isdir) {
  node->data.is_dir = isdir;
  block_write(fs_device, node->sector, &node->data);
}

int inode_openers(struct inode *node) {
  return node->open_cnt;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */

static block_sector_t byte_to_sector (const struct inode *inode, off_t pos) {
  ASSERT (inode != NULL);
  if (pos >= inode->data.length)
    return -1;
  int index = pos / BLOCK_SECTOR_SIZE;
  if(index < NUM_DATA_BLOCKS) 
    return inode->data.data_blocks[index];
  else if(index < NUM_DATA_BLOCKS + 128){
    block_sector_t buffer[128];
    block_read(fs_device, inode->data.primary_block, &buffer);
    block_sector_t s = buffer[index - NUM_DATA_BLOCKS];
    return s;
  } else if(index < NUM_DATA_BLOCKS + 128 + 128 * 128) {
    block_sector_t buffer[128];
    block_read(fs_device, inode->data.secondary_block, &buffer);
    block_sector_t first = buffer[(index - NUM_DATA_BLOCKS - 128) / 128];
    block_read(fs_device, first, &buffer);
    block_sector_t s = buffer[(index - NUM_DATA_BLOCKS - 128) % 128];
    return s;
  }
  PANIC("FILE POS LARGER THAN MAX FILE SIZE");
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  // free_map_allocate each sector separately
  // allocate primary and secondary levels correctly
  disk_inode = calloc (1, sizeof *disk_inode);
  if(!disk_inode)
    return false;
  disk_inode->length = 0;
  disk_inode->magic = INODE_MAGIC;
  bool success = inode_extend(disk_inode, sector, length);
  free(disk_inode);
  return success;
}

bool 
inode_extend(struct inode_disk *disk_inode, 
              block_sector_t sector, 
              off_t length) {
  // estend it
  static char zeros[BLOCK_SECTOR_SIZE];
  size_t sectors = bytes_to_sectors (length);
  size_t current_sectors = bytes_to_sectors (disk_inode->length);
  size_t i;
  block_sector_t *primary_data = calloc(128, sizeof(block_sector_t));
  block_sector_t *secondary_primary = calloc(128, sizeof(block_sector_t));
  block_sector_t **secondary_primary_data = calloc(128, sizeof(block_sector_t *));
  for (i = 0; NUM_DATA_BLOCKS + 128 + 128*i < sectors; i++) {
    secondary_primary_data[i] = calloc(128, sizeof(block_sector_t));
  }

  // copy everything from the current inode into the temporarruasd h
  if(disk_inode->primary_block)
    block_read (fs_device, disk_inode->primary_block, primary_data);
  if(disk_inode->secondary_block){
    block_read (fs_device, disk_inode->secondary_block, secondary_primary);
    for(i = 0; i < 128 && secondary_primary[i]; i++){
      block_read(fs_device, secondary_primary[i], secondary_primary_data[i]);
    }
  }
  for(i = current_sectors; i < sectors; i++) {
    if(i < NUM_DATA_BLOCKS) {
      // data blocks
      if (!free_map_allocate (1, &disk_inode->data_blocks[i])) return false;
      block_write (fs_device, disk_inode->data_blocks[i], &zeros);
    } else if(i < NUM_DATA_BLOCKS + 128) {
      // primary block
      if(i == NUM_DATA_BLOCKS){
        // the block itself
        if (!free_map_allocate (1, &disk_inode->primary_block)) return false;
        block_write (fs_device, disk_inode->primary_block, &zeros);
      }
      // the data
      if (!free_map_allocate (1, &primary_data[i-NUM_DATA_BLOCKS])) return false;
      block_write (fs_device, primary_data[i-NUM_DATA_BLOCKS], &zeros);
    } else if(i < NUM_DATA_BLOCKS + 128 + 128 * 128) {
      int second_index = i - (NUM_DATA_BLOCKS + 128);
      // secondary block
      if(second_index == 0){
        // the block itself
        if (!free_map_allocate (1, &disk_inode->secondary_block)) return false;
        block_write (fs_device, disk_inode->secondary_block, &zeros);
      }
      if(second_index % 128 == 0){
        // a primary block itself
        if (!free_map_allocate (1, &secondary_primary[second_index / 128])) return false;
        block_write (fs_device, secondary_primary[second_index / 128], &zeros);
      }
      // the data
      if (!free_map_allocate (1, &secondary_primary_data[second_index / 128][second_index % 128])) return false;
      block_write (fs_device, secondary_primary_data[second_index / 128][second_index % 128], &zeros);
    }
  }

  disk_inode->length = length;

  block_write (fs_device, sector, disk_inode);
  // write primary block
  if (sectors > NUM_DATA_BLOCKS && current_sectors < NUM_DATA_BLOCKS + 128)
    block_write (fs_device, disk_inode->primary_block, primary_data);
  // write secondary block
  if (sectors > NUM_DATA_BLOCKS + 128) {
    block_write (fs_device, disk_inode->secondary_block, secondary_primary);
    // write each secondary primary data array to each primary block
    for(i = 0; i < 128 && secondary_primary[i]; i++){
      block_write (fs_device, secondary_primary[i], secondary_primary_data[i]);
    }
  }

  for (i = 0; NUM_DATA_BLOCKS + 128 + 128*i < sectors; i++) {
    free(secondary_primary_data[i]);
  }
  free(primary_data);
  free(secondary_primary);
  free(secondary_primary_data);
  
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          size_t sectors = bytes_to_sectors(inode->data.length);
          if (sectors > 0) {
            size_t i;
            for (i = 0; i < sectors && i < NUM_DATA_BLOCKS; i++) {
              free_map_release(inode->data.data_blocks[i], 1);
            }
            if (sectors > NUM_DATA_BLOCKS) {
              free_map_release(inode->data.primary_block, 1);
              block_sector_t primary_data[128];
              block_read(fs_device, inode->data.primary_block, &primary_data);
              for (i = 0; !primary_data[i]; i++) {
                free_map_release(primary_data[i], 1);
              }
            }
            if (sectors > NUM_DATA_BLOCKS + 128) {
              free_map_release(inode->data.secondary_block, 1);
              block_sector_t secondary_data[128];
              block_read(fs_device, inode->data.secondary_block, &secondary_data);
              for (i = 0; !secondary_data[i]; i++) {
                free_map_release(secondary_data[i], 1);
                block_sector_t data[128];
                block_read(fs_device, secondary_data[i], &data);
                int j;
                for (j = 0; !data[i]; j++) {
                  free_map_release(data[i], 1);
                }
              }
            }
          }
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      if(!(sector_idx + 1))
        break;

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  // extend if needed
  if(byte_to_sector (inode, offset + size) == -1) {
    if(!inode_extend(&inode->data, inode->sector, offset + size))
      return 0;
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
