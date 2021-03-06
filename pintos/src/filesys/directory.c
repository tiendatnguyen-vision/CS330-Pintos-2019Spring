#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
    bool unused;
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) 
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  // printf("_____DEBUG_____inode_read_at start \n");
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
    // printf("in for loop!\n\n"); 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        // printf("name : %s\n");
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  }
  // printf("loop out!\n\n");
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  // printf("_____DEBUG____lookup start \n");
  if (lookup (dir, name, &e, NULL)) {
    // printf("_____DEBUG____lookup success \n");
    *inode = inode_open (e.inode_sector);
    if(*inode != NULL)
      (*inode)->path = name;
  }
  else 
  {
    // printf("_____DEBUG____lookup fail \n");
    *inode = NULL;
  }

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  // printf("in dir add \n");
  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  // struct inode *child_inode;
  // child_inode = inode_open(inode_sector);
  // child_inode->parent = dir->inode->sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
  // inode_close(child_inode);
  
 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  // printf("IN DIR\n");
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  // printf("dir sector %d, name %s\n", dir->inode->sector, name);
  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)) {
    // printf("NO SUCH FILE\n");
    goto done;
  }

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL) {
    // printf("NO INODE\n");
    goto done;
  }

  if (inode->isdir && !is_dir_empty(inode))
  {
    // printf("NOT EMPTY\n");
    goto done;
  }
    

  if (inode->isdir && inode->open_cnt > 2) //
  {
    // printf("inode open cnt %d\n", inode->open_cnt);
    goto done;
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  // printf("after condition\n");

  /* Remove inode. */
  inode_remove (inode);
  success = true;

  // printf("inode %08x\n", inode);
  // printf("isdir %d\n", inode->isdir);
  // printf("inode->open_cnt %d\n", inode->open_cnt);

 done:
  if (inode->isdir && inode->open_cnt == 2) {
    inode_close(inode);
  }
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          // printf("name %s\n", e.name);
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}


struct dir *
get_dir(char *path)
{
  char *name, *temp_name;
  char *ptr, *ret;
  struct dir *dir;
  struct inode *inode;
  disk_sector_t parent;
  char *slash_front_name;
  char *temp_ptr;


  char *real_path = string_pre_processing(path);
  // printf("path is %s, real_path is %s\n", path, real_path);

  name = malloc((strlen(real_path)+1)*sizeof(char));
  strlcpy(name, real_path, strlen(real_path)+1);
  ptr = strrchr(name, '/');

  if(ptr == NULL && !thread_current()->cur_dir) { // input path : abc, ., ..
    free(real_path);
    return dir_open_root();
  }
  else if(ptr == NULL)
  {
    if(strcmp(name, "..") == 0) // input path : ..
    {
      parent = thread_current()->cur_dir->inode->parent;
      free(real_path);
      return dir_open(inode_open(parent));
    }
    else // input path : abc, .
    {
      free(real_path);
      return dir_reopen(thread_current()->cur_dir);
    }
  }

  // if(ptr != name) // remove file name (only control path!) input : abc/def/123, output : abc/def
  ptr[0] = '\0';

  if((strlen(name) == 1 && name[0] == '/') || strlen(name) == 0) // root
  {
    free(real_path);
    return dir_open_root();
  }
  
  if(name[0] == '/') // input path : /abc, /123/abc  /a
  {
    dir = dir_open_root();
    temp_name = name + 1;
    ptr = strchr(temp_name, '/');
    if (ptr == NULL)
    {
      if (!dir_lookup(dir, temp_name, &inode)) {
        free(real_path);
        return NULL;
      }
      dir_close(dir);
      free(real_path);
      return dir_open(inode);
    } 
    else
    {
      slash_front_name = malloc((strlen(temp_name) + 1) * sizeof(char));
      strlcpy(slash_front_name, temp_name, strlen(temp_name)+1);
      temp_ptr = strchr(slash_front_name, '/');
      if (temp_ptr != NULL) {
        temp_ptr[0] = '\0';
      }
      if (!dir_lookup(dir, slash_front_name, &inode)) {
        free(slash_front_name);
        free(real_path);
        return NULL;
      }
      dir_close(dir);
      dir = dir_open(inode);
      free(slash_front_name);
    }
  }
  else // input path : abc/def
  {
    ptr = strchr(name, '/'); 
    dir = thread_current()->cur_dir;
    if(!dir_lookup(dir, name, &inode))
    {
      free(real_path);
      return NULL;
    }

    dir = dir_open(inode);
  }

  // printf("ptr is %s, name is %s\n", ptr, name);

  while(ptr)
  { //
    ptr[0] = '\0';
    temp_name = ptr + 1;
    if(strcmp(temp_name, ".") == 0)
    {
      ptr = strchr(temp_name, '/');
      continue;
    }
    if(strcmp(temp_name, "..") == 0)
    {
      parent = thread_current()->cur_dir->inode->parent;
      dir_close(dir);
      dir = dir_open(inode_open(parent));
      ptr = strchr(temp_name, '/'); // abc/def
      continue;
    }

    slash_front_name = malloc((strlen(temp_name) + 1) * sizeof(char));
    strlcpy(slash_front_name, temp_name, strlen(temp_name)+1);

    temp_ptr = strchr(slash_front_name, '/');
    if (temp_ptr != NULL) {
      temp_ptr[0] = '\0';
    }
    
    if(!dir_lookup(dir, slash_front_name, &inode)) {
      free(slash_front_name);
      free(real_path);
      return NULL;
    }

    dir_close(dir);
    dir = dir_open(inode);
    
    //
    free(slash_front_name);
    ptr = strchr(temp_name, '/');
  }

  free(name);
  free(real_path);
  return dir;
}

char *
get_name(char *path)
{
  char *real_path = string_pre_processing(path);
  int path_len = strlen(real_path);
  char name[path_len+1];
  char *ptr, *ret;

  strlcpy(name, real_path, path_len+1);
  ptr = strrchr(name, '/');

  if (ptr == NULL) {
    ret = malloc(strlen(real_path)+1);
    strlcpy(ret, real_path, strlen(real_path)+1);
    return ret;
  }

  ptr += 1;

  ret = malloc(strlen(ptr) * sizeof(char) + 1);
  strlcpy(ret, ptr, strlen(ptr)+1);
  free(real_path);
  return ret;
}


bool is_dir_empty(struct inode *inode)
{
  struct dir_entry e;
  off_t pos = 0;

  while (inode_read_at (inode, &e, sizeof e, pos) == sizeof e) 
    {
      pos += sizeof e;
      if (e.in_use)
        {
          return false;
        } 
    }
  return true;
}


char *
string_pre_processing(char * path) // input //a/b/c//d => /a/b/c/d
{
  char *ptr, *ret, *temp;
  int cnt, i, len = 0;

  temp = malloc(strlen(path) + 1);
  ret = malloc(strlen(path) + 1);

  // if(strlen(path) == 0)
  //   return NULL;

  strlcpy(temp, path, strlen(path) + 1);
  ptr = strstr(temp, "//");
  
  while(ptr)
  {
    cnt = ptr - temp;
    for(i=0; i<cnt+1; i++)
    {
      *(ret+len) = *(temp+i);
      len++;
    }
    temp = ptr + 2;
    ptr = strstr(temp, "//");
  }

  for(i=0; i<strlen(temp); i++)
  {
    *(ret+len) = *(temp+i);
    len++;
  }

  *(ret+len) = '\0';

  free(temp);
  return ret;
}