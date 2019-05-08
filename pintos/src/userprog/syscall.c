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

static void syscall_handler (struct intr_frame *);
int sys_write(int fd, const void *buffer, unsigned size);
void* valid_pointer(void *ptr);

struct lock file_lock;
extern struct lock swap_lock;

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
  }
}

void* valid_pointer(void *ptr) {
  // syscall에서 code segment에 write..?

  if(ptr == NULL || !is_user_vaddr(ptr) || ptr < (void *) 0x08048000)
	{
    // printf("null exit\n");
    exit(-1);
	}
  struct thread *curr = thread_current();
  ////////////////////////////////////////////////////////////////
  //page_fault 핸들링, 이상하게 read의 buffer_addr가 valid한지는 page fault가 안나옴 여기서 처리해야 할듯?!

  /* _____________________________________________ */
  // void *upage = pg_round_down (ptr);
  // // Stack grow인 경우
  // /* if (need_stack_grow_in_syscall(ptr)) {
  //   void *kpage = palloc_get_page (PAL_USER);
  //   stack_grow(upage, kpage);
  // } */
  // // Map page with frame
  // if (spte_find(upage) && pagedir_get_page(curr->pagedir, ptr) == NULL) { 
  //   if (ptr > 0x90000000) {
  //     exit(-1);
  //   }
  //   // void *upage = pg_round_down (fault_addr);
  //   struct sup_page_table_entry *find_spte = spte_find(upage);
  //   // if (find_spte == NULL) exit(-1);

  //   /* Get kpage to allocate frame */
  //   void *kpage;
  //   if (find_spte->page_read_bytes != 0)
  //     kpage = palloc_get_page (PAL_USER);
  //   else
  //     kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  //   /* Have kpage to allocate frame */
  //   if (kpage != NULL) {
  //     /* Lazy loading */
  //     if (find_spte->from_load) {
  //       load_file_lazily(kpage, find_spte);
  //     }

  //     /* Add kpage to frame */
  //     struct frame_table_entry *new_fte = allocate_frame(kpage, find_spte);
  //     if (new_fte == NULL) free_kpage_and_exit(kpage);
  //     if (install_page(upage, kpage, find_spte->writable)) {
  //       find_spte->frame = kpage;
  //       find_spte->is_in_frame = 1;
  //     } else {
  //       free_kpage_and_exit(kpage);
  //     }
  //   }
  //   else { /* Frame eviction is needed */
  //     if (!find_spte->is_mapped) {
  //       swap_out();
  //       if(find_spte->page_read_bytes > 0)
  //         kpage = palloc_get_page(PAL_USER);
  //       else
  //         kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  //       lock_release(&swap_lock);

  //       load_file_lazily(kpage, find_spte);
        
  //       struct frame_table_entry *new_fte = allocate_frame(kpage, find_spte);
  //       if (new_fte == NULL) free_kpage_and_exit(kpage);
  //       if (install_page(upage, kpage, find_spte->writable)) {
  //         find_spte->frame = kpage;
  //         find_spte->is_in_frame = 1;
  //       } else {
  //         free_kpage_and_exit(kpage);
  //       }
  //     }
  //     else if (find_spte->is_in_swap) { /* page data is in swap */
  //       kpage = evict_frame(upage);
  //       // if (!install_page(upage, kpage, find_spte->writable)) free_kpage_and_exit(kpage);
  //     }
  //     else { /* page data is in file */

  //     }
  //   }
  // }
  // else if (need_stack_grow_in_syscall(ptr)) {
  //   printf("stack \n");
  //   void *kpage = palloc_get_page (PAL_USER);
  //   stack_grow(upage, kpage);
  // } 
  /* _____________________________________________ */
  void *fault_addr = pg_round_down(ptr);
  struct sup_page_table_entry *find_spte = spte_find(fault_addr);
  if(find_spte && pagedir_get_page(curr->pagedir, ptr) == NULL)
  {
    void *upage = fault_addr;
    uint8_t *kpage;

    if(find_spte->page_read_bytes != 0)
      kpage = palloc_get_page (PAL_USER);
    else
      kpage = palloc_get_page (PAL_USER | PAL_ZERO);

    if (kpage != NULL) {
      if (find_spte->from_load && find_spte->page_read_bytes > 0) {
        lock_acquire(&file_lock);
        if (file_read_at (find_spte->file, kpage, find_spte->page_read_bytes, find_spte->ofs) != (int) find_spte->page_read_bytes)
        {
          palloc_free_page (kpage);
          lock_release(&file_lock);
          exit(-1);
        }
        memset (kpage + find_spte->page_read_bytes, 0, find_spte->page_zero_bytes);
        lock_release(&file_lock);
      }

      // frame table에 추가
      struct frame_table_entry *new_fte = allocate_frame(kpage, find_spte);
      if (new_fte == NULL)
      {
        palloc_free_page(kpage);
        exit(-1);
      }

      if (!install_page(upage, kpage, find_spte->writable)) {
        palloc_free_page(kpage);
        exit(-1);
      }
      find_spte->frame = kpage;
    }
  }
  // 추가로 여기에 stack growth 도 해줘야하는가? 지금 pt-grow-stk-sc가 여기인거같은데..? stack syscall = stk-sc??
  ////////////////////////////////////////////////////////////////
  if (pagedir_get_page(curr->pagedir, ptr) == NULL)
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
  return filesys_create (file, initial_size); 
}

bool remove (const char *file)
{
  return filesys_remove(file);
}

int open (const char *file)
{
  //
  struct thread *curr = thread_current();
  struct file_info *new_file_info = palloc_get_page(0); // we do not handle ended fd_info without meating syscall close? (close do page free !!!!!!!!!!!!)

  if(new_file_info == NULL){
    palloc_free_page(new_file_info);
    return -1;
  }

  lock_acquire(&file_lock);
  struct file *new_file = filesys_open(file);
  lock_release(&file_lock);

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

  int num_write = file_write(fd_info->file, buffer, length);

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
