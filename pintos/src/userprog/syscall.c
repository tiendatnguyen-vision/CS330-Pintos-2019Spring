#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "threads/init.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/file.h"

#define READDIR_MAX_LEN 14

static void syscall_handler (struct intr_frame *);
int sys_write(int fd, const void *buffer, unsigned size);
void* valid_pointer(void *ptr);

struct lock file_lock;

bool need_stack_grow_in_syscall (void *fault_addr)
{
  if ((thread_current()->user_esp - 32 <= fault_addr) && fault_addr >= 0x90000000){
    return true;
  }
  return false;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *valid_syscall_num = (int *)valid_pointer((void*)(f->esp));
  int syscall_num = *valid_syscall_num;
  thread_current()->user_esp = f->esp;

  switch(syscall_num){
    case SYS_HALT:
    {
      power_off();
      break;
    }
    case SYS_EXIT:
    {
      int *valid_status = (int *)valid_pointer((void*)(f->esp+4));
      exit(*valid_status);
      break;
    }
    case SYS_EXEC:
    {
      int *valid_file_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_file= (int *)valid_pointer((void *)*valid_file_addr); // esp 안의 값(주소)가 valid한지 확인
      f->eax = exec((const char *)*valid_file_addr);
      break;
    }
    case SYS_WAIT:
    {
      int *valid_pid = (int *)valid_pointer((void*)(f->esp+4));
      f->eax = wait((pid_t)*valid_pid);
      break;
    }
    case SYS_CREATE:
    {
      int *valid_file_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_file= (int *)valid_pointer((void *)*valid_file_addr); // esp 안의 값(주소)가 valid한지 확인
      int *valid_size = (int *)valid_pointer((void *)(f->esp+8));
      f->eax = create((const char *)*valid_file_addr, (unsigned)*valid_size);
      break;
    }
    case SYS_REMOVE:
    {
      int *valid_file_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_file= (int *)valid_pointer((void *)*valid_file_addr); // esp 안의 값(주소)가 valid한지 확인
      f->eax = remove((const char *)*valid_file_addr);
      break;
    }
    case SYS_OPEN:
    {
      int *valid_file_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_file= (int *)valid_pointer((void *)*valid_file_addr);
      f->eax = open((const char*)*valid_file_addr);
      break;
    }
    case SYS_FILESIZE:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      f->eax = filesize(*valid_fd);
      break;
    }
    case SYS_READ:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      int *valid_buffer_addr = (int *)valid_pointer((void*)(f->esp+8));
      int *valid_buffer = (int *)valid_pointer((void *)*valid_buffer_addr); // esp 안의 값(주소)가 valid한지 확인
      int *valid_size = (int *)valid_pointer((void*)(f->esp+12));
      f->eax = read(*valid_fd, (const void *)*valid_buffer_addr, (unsigned)*valid_size);
      break;
    }
    case SYS_WRITE:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      int *valid_buffer_addr = (int *)valid_pointer((void*)(f->esp+8));
      int *valid_buffer = (int *)valid_pointer((void *)*valid_buffer_addr);
      int *valid_length = (int *)valid_pointer((void*)(f->esp+12));
      f->eax = write(*valid_fd, (const void *)*valid_buffer_addr, (unsigned)*valid_length);
      break;
    }
    case SYS_SEEK:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      int *valid_position = (int *)valid_pointer((void*)(f->esp+8));
      seek(*valid_fd, (unsigned)*valid_position);
      break;
    }
    case SYS_TELL:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      f->eax = tell(*valid_fd);
      break;
    }
    case SYS_CLOSE:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      close(*valid_fd);
      break;
    }
    case SYS_MMAP:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      int *valid_buffer_addr = (int *)valid_pointer((void*)(f->esp+8));
      // int *valid_buffer = (int *)valid_pointer((void *)*valid_buffer_addr);
      f->eax = mmap(*valid_fd, (const void *)*valid_buffer_addr);
      break;
    }
    case SYS_MUNMAP:
    {
      int *valid_mapid = (int*)valid_pointer((void*)(f->esp+4));
      munmap(*valid_mapid);
      break;
    }
    case SYS_MKDIR:
    {
      int *valid_dir_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_dir = (int *)valid_pointer((void *)*valid_dir_addr);
      f->eax = mkdir((const char*)*valid_dir_addr);
      break;
    }
    case SYS_CHDIR:
    {
      int *valid_dir_addr = (int *)valid_pointer((void *)(f->esp+4)); 
      int *valid_dir = (int *)valid_pointer((void *)*valid_dir_addr);
      f->eax = chdir((const char*)*valid_dir_addr);
      break;
    }
    case SYS_ISDIR:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      f->eax = isdir(*valid_fd);
      break;
    }
    case SYS_INUMBER:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      f->eax = inumber(*valid_fd);
      break;
    }
    case SYS_READDIR:
    {
      int *valid_fd = (int*)valid_pointer((void*)(f->esp+4));
      int *valid_name_addr = (int *)valid_pointer((void*)(f->esp+8));
      int *valid_name = (int *)valid_pointer((void *)*valid_name_addr); // esp 안의 값(주소)가 valid한지 확인
      f->eax = readdir(*valid_fd, (char *)*valid_name_addr);
      break;
    }
  }
}

void* valid_pointer(void *ptr) {
  // syscall에서 code segment에 write..?
  if(ptr == NULL || !is_user_vaddr(ptr) || ptr < (void *) 0x08048000)
	{
    exit(-1);
	}
  struct thread *curr = thread_current();
  ////////////////////////////////////////////////////////////////
  //page_fault 핸들링, 이상하게 read의 buffer_addr가 valid한지는 page fault가 안나옴 여기서 처리해야 할듯?!

  void *upage = pg_round_down (ptr);

  // // Map page with frame
  if (spte_find(upage) && pagedir_get_page(curr->pagedir, ptr) == NULL) { 
    if (ptr > 0x90000000) {
      exit(-1);
    }
    struct sup_page_table_entry *find_spte = spte_find(upage);

    /* Get kpage to allocate frame */
    void *kpage;
    if (find_spte->page_read_bytes != 0)
      kpage = palloc_get_page (PAL_USER);
    else
      kpage = palloc_get_page (PAL_USER | PAL_ZERO);

    /* Have kpage to allocate frame */
    if (kpage != NULL) {
      /* Lazy loading */
      load_file_lazily(kpage, find_spte);

      /* Add kpage to frame */
      struct frame_table_entry *new_fte = allocate_frame(kpage, find_spte);
      if (new_fte == NULL) free_kpage_and_exit(kpage);
      if (install_page(upage, kpage, find_spte->writable)) {
        find_spte->frame = kpage;
        find_spte->is_in_frame = 1;
      } else {
        free_kpage_and_exit(kpage);
      }
    }
    else { /* Frame eviction is needed */
      if (!find_spte->is_mapped) {
        swap_out();
        if(find_spte->page_read_bytes > 0)
          kpage = palloc_get_page(PAL_USER);
        else
          kpage = palloc_get_page(PAL_USER | PAL_ZERO);
        while(!kpage)
        {
          swap_out();
          if(find_spte->page_read_bytes > 0)
            kpage = palloc_get_page(PAL_USER);
          else
            kpage = palloc_get_page(PAL_USER | PAL_ZERO);
        }
        load_file_lazily(kpage, find_spte);

        struct frame_table_entry *new_fte = allocate_frame(kpage, find_spte);
        if (new_fte == NULL) free_kpage_and_exit(kpage);
        if (install_page(upage, kpage, find_spte->writable)) {
          find_spte->frame = kpage;
          find_spte->is_in_frame = 1;
        } else {
          free_kpage_and_exit(kpage);
        }
      }
      else { /* page data is in swap or file */
        kpage = evict_frame(upage);
      }
    }
  }
  else if (need_stack_grow_in_syscall(ptr) && pagedir_get_page(curr->pagedir, ptr) == NULL) {
    void *kpage = palloc_get_page (PAL_USER);
    stack_grow(upage, kpage);
  } 
  // 추가로 여기에 stack growth 도 해줘야하는가? 지금 pt-grow-stk-sc가 여기인거같은데..? stack syscall = stk-sc??
  ////////////////////////////////////////////////////////////////
  else if (pagedir_get_page(curr->pagedir, ptr) == NULL)
  {
    exit(-1);
  }
  if(!(ptr > 0x80480a0UL)) {
    exit(-1);
  }
  return ptr;
}

void exit (int status)
{
  struct thread *curr_thread = thread_current();
  curr_thread->exit_status = status;
  thread_exit();
}

pid_t exec (const char *file)
{
  pid_t temp;
  temp = process_execute(file);
  return temp;
}

int wait (pid_t pid)
{
  int child_exit_status = process_wait(pid);
  return child_exit_status;
}

bool create (const char *file, unsigned initial_size)
{
  bool return_value;
  lock_acquire(&file_lock);
  return_value = filesys_create (file, initial_size); 
  lock_release(&file_lock);
  return return_value;
}

bool remove (const char *file)
{
  return filesys_remove(file);
}

int open (const char *file)
{
  //
  struct thread *curr = thread_current();
  struct file_info *new_file_info = palloc_get_page(0); // we do not handle ended fd_info without meeting syscall close? (close do page free !!!!!!!!!!!!)

  if(new_file_info == NULL){
    palloc_free_page(new_file_info);
    return -1;
  }

  lock_acquire(&file_lock);
  struct file *new_file = filesys_open(file);
  lock_release(&file_lock);

  // printf("after open!\n");

  if (new_file == NULL) {
    return -1;
  }

  if (strcmp(thread_current()->name, file) == 0) {
    file_deny_write(new_file);
  }

  new_file_info->fd = ++curr->user_fd;
  new_file_info->file = new_file;

  list_push_back(&curr->fd_list, &new_file_info->elem);
  return new_file_info->fd;
}

int filesize (int fd) 
{
  //
  lock_acquire(&file_lock);
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }
  int file_size = file_length(fd_info->file);
  lock_release(&file_lock);
  return file_size;
}

int read (int fd, void *buffer, unsigned size)
{
  //
  int i;
  for(i=1;i<size/4096 + 1;i++)
    valid_pointer((void *)(buffer + i * 4096));
    
  lock_acquire(&file_lock);

  if (fd==0) {
    int i;
    for (i=0; i < size; i++) {
      *((char *)buffer++) = input_getc();
    }
    lock_release(&file_lock);
    return size;
  }

  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }
  int num_read = file_read(fd_info->file, buffer, size);
  lock_release(&file_lock);
  return num_read;
}

int write (int fd, const void *buffer, unsigned length)
{
  int i;
  int num_write = -1;
  for(i=1;i<length/4096 + 1;i++)
    valid_pointer((void *)(buffer + i * 4096));

  lock_acquire(&file_lock);
  if (fd == 1) {
    putbuf(buffer, length);
    lock_release(&file_lock);
    return length;
  }
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }

  if(!inode_isdir(file_get_inode(fd_info->file)))
  {
    // printf("inode is dir %d\n", inode_isdir(file_get_inode(fd_info->file)));
    num_write = file_write(fd_info->file, buffer, length);
  }
    
  lock_release(&file_lock);

  return num_write;
}

void seek (int fd, unsigned position) 
{
  //
  lock_acquire(&file_lock);
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }
  file_seek(fd_info->file, position);
  lock_release(&file_lock);
}

unsigned tell (int fd) 
{
  //
  lock_acquire(&file_lock);
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }
  unsigned position = file_tell(fd_info->file);
  lock_release(&file_lock);
  return position;
}

void close (int fd)
{
  lock_acquire(&file_lock);
  int has_fd = 0;
  struct thread *curr = thread_current();
  struct list_elem *e, *next;

  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file_info *fd_info;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      has_fd = 1;
      break;
    }
  }
  if (has_fd == 0) {
    exit(-1);
  }
  file_close(fd_info->file);
  list_remove(&fd_info->elem);
  palloc_free_page(fd_info);
  lock_release(&file_lock);
}

mapid_t mmap(int fd, void *addr)
{
  // lock_acquire(&file_lock);
  /* Handling fail case: file descriptors is 0 or 1. */
  if (fd == 0 || fd == 1) {
    // lock_release(&file_lock);
    return -1;
  }
  /* Find fd in the current thread's file list */
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    // lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    // lock_release(&file_lock);
    return -1;
  }
  lock_acquire(&file_lock);
  struct file *file = file_reopen(fd_info->file);
  /* Handling exit(-1) case
  1. file has zero bytes
  2. addr is not page-aligned, not user_vaddr
  3. addr is 0 
  4. addr is in stack segment(not loaded, but stack grow regin */
  if (file_length(file) == 0 || pg_ofs(addr) != 0 || addr == 0 || !is_user_vaddr(addr) || addr >= 0x90000000) {
    lock_release(&file_lock);
    return -1;
  }

  /* Load page lazily in file */
  int file_size = file_length(file);
  int ofs = 0;
  int writable = true;
  int return_mapid = curr->mapid++; //modify
  void *upage = pg_round_down(addr);
  while (file_size > 0) {
    /* Do calculate how to fill this page.
        We will read PAGE_READ_BYTES bytes from FILE
        and zero the final PAGE_ZERO_BYTES bytes. */
    
    size_t page_read_bytes = file_size < PGSIZE ? file_size : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    upage = pg_round_down(upage);
    /* Check whether this file is mmap */
    struct sup_page_table_entry *find_spte = spte_find(upage);
  
    if (find_spte != NULL) {
      lock_release(&file_lock);
      return -1;
    }
    // (void *addr, void *frame, bool is_in_frame, bool is_in_swap, struct file *file, off_t ofs, size_t page_read_bytes, size_t page_zero_bytes, bool writable, bool from_load);
    find_spte = allocate_page(upage, 0, 0, 0, file, ofs, page_read_bytes, page_zero_bytes, writable, 0); // Load하면 frame에 있다. 얘는 file 공간에서 온 애니까..
    
    /* to pass mmap test - 코드 지저분해도 이해 부탁 */
    find_spte->page_read_bytes = page_read_bytes;

    struct mfile *mfile = malloc(sizeof(struct mfile));
    mfile->mapid = return_mapid;
    mfile->upage = upage;
    mfile->fd_info = fd_info;
    
    list_push_back(&curr->mfile_list, &mfile->elem);

    /* Advance. */
    file_size -= page_read_bytes;
    upage += PGSIZE;
    ofs += page_read_bytes;
  }
  lock_release(&file_lock);
  return return_mapid;
}
  
void munmap(mapid_t mapid)
{
  // /* 1. unmap the mapping  */
  // printf("ummap?\n");
  struct thread *curr = thread_current();
  struct mfile *find_mfile;
  struct list_elem *e, *next;

  if (list_empty(&curr->mfile_list)) return;

  lock_acquire(&file_lock);
  for (e=list_begin(&curr->mfile_list); e != list_tail(&curr->mfile_list); e = next)
  {
    next = list_next(e);
    find_mfile = list_entry(e, struct mfile, elem);
    if (find_mfile->mapid == mapid) {
      // find = 1;
      list_remove(&find_mfile->elem);

      void *kpage;
      struct sup_page_table_entry *find_spte = spte_find(find_mfile->upage);

      if (pagedir_is_dirty(curr->pagedir, find_spte->user_vaddr)) {
        if (!pagedir_get_page(curr->pagedir, find_spte->user_vaddr))
          kpage = evict_frame(find_mfile->upage);
        else
          kpage = find_spte->frame;
        file_write_at(find_spte->file, kpage, find_spte->page_read_bytes, find_spte->ofs);
      }
      // /* Erase fte and spte together */
      if(find_spte->is_mapped && find_spte->is_in_frame == 1 && find_spte->frame != 0)
        remove_frame(find_spte->frame);
      else
      {
        hash_delete(&curr->spt, &find_spte->hash_elem);
        free(find_spte);
      }
      free(find_mfile);
    }
  }
  lock_release(&file_lock);
}

void mummap_all()
{
  // /* 1. unmap the mapping  */
  struct thread *curr = thread_current();
  struct mfile *find_mfile;
  struct list_elem *e, *next;

  if (list_empty(&curr->mfile_list)) return;

  lock_acquire(&file_lock);

  for (e=list_begin(&curr->mfile_list); e != list_tail(&curr->mfile_list); e = next)
  {
    next = list_next(e);
    find_mfile = list_entry(e, struct mfile, elem);

    list_remove(&find_mfile->elem);
    // printf("find_mfile's upage is %08x, and mapid is %d\n", find_mfile->upage, find_mfile->mapid);
    void *kpage;
    struct sup_page_table_entry *find_spte = spte_find(find_mfile->upage);

    if (pagedir_is_dirty(curr->pagedir, find_spte->user_vaddr)) {
      if (!pagedir_get_page(curr->pagedir, find_spte->user_vaddr))
        kpage = evict_frame(find_mfile->upage);
      else
        kpage = find_spte->frame;
      file_write_at(find_spte->file, kpage, find_spte->page_read_bytes, find_spte->ofs);
    }

    // /* Erase fte and spte together */
    if(find_spte->is_mapped && find_spte->is_in_frame == 1 && find_spte->frame != 0)
      remove_frame(find_spte->frame);
    else
    {
      hash_delete(&curr->spt, &find_spte->hash_elem);
      free(find_spte);
    }
    free(find_mfile);
  }
  lock_release(&file_lock);


  // frame table mapping을 지워주기
  frame_free_mapping_with_curr_thread(curr);
}

bool mkdir(const char *dir)
{
  if (strlen(dir) == 0) {
    return false;
  }
  bool return_value;
  lock_acquire (&file_lock);

  return_value = filesys_create(dir, 0);
  
  struct dir *file_dir = get_dir(dir);
  char *name = get_name(dir);
  struct inode *inode;
  if (!dir_lookup(file_dir, name, &inode)) {
    dir_close(file_dir);
    return false;
  }
  inode->isdir = 1;
  inode->parent = inode_get_inumber(dir_get_inode(file_dir));
  inode->path = dir;
  // printf("child's parent sector is %zu\n", inode->parent);
  // printf("inode open count %d\n", inode->open_cnt);
  // inode_close(inode); // for dir_lookup  <<<<<<<<<<<<<<<<<<<<<<<<< 생각해보기
  lock_release (&file_lock);
  
  // printf("dir open cnt %d\n", inode_open_cnt(dir_get_inode(file_dir)));

  dir_close(file_dir); // for get_dir
  free(name);
  return return_value;
}

bool chdir(const char *dir)
{
  lock_acquire(&file_lock);

  struct dir *file_dir = get_dir(dir);
  struct dir *final_dir;
  char *name = get_name(dir);
  struct inode *inode;

  // printf("dir sector : %zu\n", inode_get_inumber(dir_get_inode(file_dir)));

  if(inode_get_inumber(dir_get_inode(file_dir)) == 1 && strlen(name) == 0) //root
  {
    dir_close(thread_current()->cur_dir);
    final_dir = dir_open(dir_get_inode(file_dir));
    thread_current()->cur_dir = final_dir;
  }
  else if(strcmp(name, ".") == 0)
  {
    dir_close(thread_current()->cur_dir);
    dir_open(dir_get_inode(file_dir));
    thread_current()->cur_dir = dir;
  }
  else if(strcmp(name, "..") == 0)
  {
    // final_dir = dir_open(inode_open(inode_parent(dir_get_inode(file_dir))));
    final_dir = dir_reopen(file_dir);
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = final_dir;
  }
  else
  {
    if(!dir_lookup(file_dir, name, &inode))
    {
      lock_release(&file_lock);
      return false;
    }
    final_dir = dir_open(inode);
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = final_dir;
  }

  dir_close(file_dir);
  lock_release(&file_lock);
  return true;
}

bool isdir (int fd)
{
  lock_acquire(&file_lock);
  bool return_value;
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return false;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return false;
  }

  return_value = inode_isdir(file_get_inode(fd_info->file));
  lock_release(&file_lock);
  return return_value;
}

int inumber (int fd)
{
  lock_acquire(&file_lock);
  int return_value;
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return -1;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return -1;
  }

  // printf("fd is %d, fd_info->file %08x, inode %08x, inumber %d\n",fd, fd_info->file, file_get_inode(fd_info->file), inode_get_inumber(file_get_inode(fd_info->file)));

  return_value = inode_get_inumber(file_get_inode(fd_info->file));
  lock_release(&file_lock);
  return return_value;
}

bool
readdir (int fd, char name[READDIR_MAX_LEN + 1]) 
{
  lock_acquire(&file_lock);
  bool return_value;
  struct thread *curr = thread_current();
  struct list_elem *e, *next;
  if (list_empty(&curr->fd_list)) {
    lock_release(&file_lock);
    return false;
  }
  struct file_info *fd_info;
  int find = 0;
  for (e=list_begin(&curr->fd_list); e != list_end(&curr->fd_list); e = next) {
    next = list_next(e);
    fd_info = list_entry(e, struct file_info, elem);
    if (fd_info->fd == fd) {
      find = 1;
      break;
    }
  }
  if (find == 0) {
    lock_release(&file_lock);
    return false;
  }

  struct inode *inode = file_get_inode(fd_info->file);
  if (!inode_isdir(inode)) {
    lock_release(&file_lock);
    return false;
  }

  // struct dir *open_dir = dir_open(inode);
  // return_value = dir_readdir (open_dir, name);
  // dir_close(open_dir);
  struct dir *open_dir = (struct dir*)fd_info->file;
  return_value = dir_readdir(open_dir, name);

  lock_release(&file_lock);
  return return_value;
}