#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* Lock for syscalls dealing with critical sections of files. */
struct lock file_lock;

/* Lock for syscalls dealing with reading/writing from system. */
struct lock sys_lock;

/* Lock for adding to threads child process list. */
struct lock child_process_lock;

static void syscall_handler (struct intr_frame *);
static struct thread_open_file *find_thread_open_file (int);
static struct thread *find_child_thread (pid_t);
static bool is_page_mapped (void *);
static void check_valid_user_vaddr (const void *);
static void check_valid_buffer (void *, unsigned);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
  lock_init (&sys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{  
  check_valid_user_vaddr ((void *)((int *)f->esp + 1));

  /*
    Acts on various syscalls. Calls appropriate function. Extracts and passes args.
    If function returns anything, value is stored in eax register.
  */
  int sys_code = *(int *)f->esp;

  switch (sys_code)
  {
    case SYS_HALT:
    {
      halt ();
      break;
    }
    case SYS_EXIT:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      int status = *((int *)f->esp + 1);
      exit (status);
      break;
    }
    case SYS_EXEC:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      void *cmd_line = (void *)(*((int*)f->esp + 1));
      f->eax = exec((const char *)cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      int p_id = *((int *)f->esp + 1);
      f->eax = wait(p_id);
      break;
    }
    case SYS_CREATE:
    {
      check_valid_user_vaddr ((int *)f->esp + 3);
      void *file = (void *)(*((int*)f->esp + 1));
      unsigned initial_size = *((unsigned *)f->esp + 2);
      f->eax = create((const char *)file, initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      void *file = (void *)(*((int*)f->esp + 1));
      f->eax = remove(file);
      break;
    }
    case SYS_OPEN:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      void *file = (void *)(*((int*)f->esp + 1));
      f->eax = open((const char *)file);
      break;
    }
    case SYS_FILESIZE:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      int fd = *((int *)f->esp + 1);
      f->eax = filesize(fd);
      break;
    }
    case SYS_READ:
    {
      check_valid_user_vaddr ((int *)f->esp + 4);
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int*)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      check_valid_buffer (buffer, size);
      f->eax = read (fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      check_valid_user_vaddr ((int *)f->esp + 4);
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int*)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      check_valid_buffer (buffer, size);
      f->eax = write(fd, buffer, size);
      break;
    }
    case SYS_SEEK:
    {
      check_valid_user_vaddr ((int *)f->esp + 3);
      int fd = *((int *)f->esp + 1);
      unsigned position = *((unsigned *)f->esp + 2);
      seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      int fd = *((int *)f->esp + 1);
      f->eax = tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
      check_valid_user_vaddr ((int *)f->esp + 2);
      int fd = *((int *)f->esp + 1);
      close(fd);
      break;
    }
  }
}

void
halt (void)
{
  shutdown_power_off ();
}

/* 
  Prints to console the current threads name and status when exiting based off what was passed in
  and then exits via thread_exit().
*/
void
exit (int status)
{
  struct thread *cur = thread_current ();
  cur->exit_status = status;
  printf ("%s: exit(%d)\n", cur->name, status);
  thread_exit ();
}

pid_t
exec (const char *cmd_line)
{
  if (cmd_line == NULL)
    return -1;
  return process_execute (cmd_line);
}

int
wait (pid_t pid)
{
  return process_wait (pid);
}

/*
  We first acquire a lock since this is File I/O.
  Then we call filesys_create, which creates a new file and returns true if successful.
  Lock then is released and filesys_create return value is returned.
*/
bool
create (const char *file, unsigned initial_size)
{
  if (file == NULL || pagedir_get_page (thread_current ()->pagedir, file) == NULL)
    exit (-1);

  lock_acquire(&file_lock);

    bool create_successful = filesys_create(file, initial_size);

  lock_release(&file_lock);

  return create_successful;
}

/*
  We acquire a lock first since this is File I/O.
  Then we call filesys_remove, which removes a file given a filename and returns true if successful.
  Lock is then released and the return value of filesys_remove is returned.
*/
bool
remove (const char *file)
{
  lock_acquire(&file_lock);

    bool remove_successful = filesys_remove(file);

  lock_release(&file_lock);

  return remove_successful;
}

int
open (const char *file)
{
  if (file == NULL || pagedir_get_page (thread_current ()->pagedir, file) == NULL)
    exit (-1);

  lock_acquire (&file_lock);
  struct file *f = filesys_open (file);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return -1;
  }
  else
  {
    int cur_fd = thread_current ()->fd_counter;
    struct thread_open_file *new_file = malloc (sizeof(struct thread_open_file));
    new_file->fd = cur_fd;
    new_file->file = f;
    list_push_front(&thread_current ()->open_files, &new_file->elem);
    thread_current ()->fd_counter += 1;

    lock_release (&file_lock);
    return cur_fd;
  }
}

int
filesize (int fd)
{
  lock_acquire (&file_lock);

    struct thread_open_file *tof = find_thread_open_file (fd);
    if (tof == NULL)
      return -1;
    struct file *f = (struct file *)(tof->file);
    int file_size = file_length (f);
  
  lock_release (&file_lock);

  return file_size;
}

int
read (int fd, void *buffer, unsigned size)
{
  /* Cannot read from STDOUT. */
  if (fd == STDOUT_FILENO)
    return 0;

  /* Read from STDIN. */
  if (fd == STDIN_FILENO)
  {
    lock_acquire (&sys_lock);

      uint8_t *buff_ptr = (uint8_t *)buffer;
      for(unsigned i = 0; i < size; i++)
      {
        buff_ptr[i] = input_getc();
      }

    lock_release (&sys_lock);

		return size;
  }

  lock_acquire (&file_lock);

    struct thread_open_file *tof = find_thread_open_file (fd);
    if (tof == NULL)
      exit (-1);
    struct file *f = (struct file *)(tof->file);
    int bytes_read = file_read (f, buffer, size);
  
  lock_release (&file_lock);

  return bytes_read;
}

int
write (int fd, const void *buffer, unsigned size)
{
  /* Disallow write to STDIN. */
  if (fd == STDIN_FILENO)
    exit (-1);

  /* Write to STDOUT. */
  if (fd == STDOUT_FILENO)
  {
    putbuf ((const char *)buffer, (size_t)size);
    return size;
  }
  
  lock_acquire (&file_lock);

    struct thread_open_file *tof = find_thread_open_file (fd);
    if (tof == NULL)
      exit (-1);
    struct file *f = (struct file *)(tof->file);
    // TODO: Deny write to executable files.
    int bytes_written = (int)file_write (f, buffer, (off_t)size);

  lock_release (&file_lock);
  return bytes_written;
}

void
seek (int fd, unsigned position)
{
  lock_acquire (&file_lock);
  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    struct file *f = (struct file *)(tof->file);
    file_seek (f, position);
  }
  lock_release (&file_lock);
}

unsigned
tell (int fd)
{
  lock_acquire (&file_lock);
  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    struct file *f = (struct file *)(tof->file);
    unsigned position = file_tell (f);
    lock_release (&file_lock);
    return position;
  }

  lock_release (&file_lock);
  return -1;
}

void
close (int fd)
{
  lock_acquire (&file_lock);

  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    file_close ((struct file *)(tof->file));
    list_remove (&tof->elem);
    free (tof);
  }
  
  lock_release (&file_lock);
}

static struct thread_open_file *
find_thread_open_file (int fd)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->open_files); e != list_end (&cur->open_files); e = list_next (e))
  {
    struct thread_open_file *tof = list_entry (e, struct thread_open_file, elem);

    if (tof->fd == fd)
      return tof;
  }

  return NULL;
}

static bool
is_page_mapped (void *check_vaddr)
{
  void *ptr = pagedir_get_page (thread_current ()->pagedir, check_vaddr);

  if (ptr == NULL)
    return false;
  
  return true;
}

/* Exits with error status if vaddr is invalid. */
static void
check_valid_user_vaddr (const void *check_vaddr)
{
  if (check_vaddr == NULL                 || 
      check_vaddr < ((void *)0x08048000)  || 
      !is_user_vaddr (check_vaddr)        || 
      !is_page_mapped(check_vaddr))
  {
    exit (-1);
  }
}

/* 
  Ensures that entire buffer is valid in memory.
  This function inspired by:
   -https://github.com/ChristianJHughes/pintos-project2/blob/master/pintos/src/userprog/syscall.c
*/
static void
check_valid_buffer (void *buffer, unsigned size)
{
  char *buff_ptr = (char *)buffer;
  for (unsigned i = 0; i < size; i++)
  {
    check_valid_user_vaddr (buff_ptr);
    buff_ptr += 1;
  }
}