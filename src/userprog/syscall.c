#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
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

static void syscall_handler (struct intr_frame *);
static struct thread_open_file *find_thread_open_file (int);
static bool is_valid_user_vaddr (void *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if (!is_valid_user_vaddr (f->esp))
    exit (-1);
  
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
      int status = *((int *)f->esp + 1);
      exit (status);
      break;
    }
    case SYS_EXEC:
    {
      void *cmd_line = (void *)(*((int*)f->esp + 1));
      f->eax = exec((const char *)cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      int p_id = *((int *)f->esp + 1);
      f->eax = wait(p_id);
      break;
    }
    case SYS_CREATE:
    {
      void *file = (void *)(*((int*)f->esp + 1));
      unsigned initial_size = *((unsigned *)f->esp + 2);
      f->eax = create((const char *)file, initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      void *file = (void *)(*((int*)f->esp + 1));
      f->eax = remove(file);
      break;
    }
    case SYS_OPEN:
    {
      void *file = (void *)(*((int*)f->esp + 1));
      f->eax = open((const char *)file);
      break;
    }
    case SYS_FILESIZE:
    {
      int fd = *((int *)f->esp + 1);
      f->eax = filesize(fd);
      break;
    }
    case SYS_READ:
    {
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int*)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int*)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = write(fd, buffer, size);
      break;
    }
    case SYS_SEEK:
    {
      int fd = *((int *)f->esp + 1);
      unsigned position = *((unsigned *)f->esp + 2);
      seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      int fd = *((int *)f->esp + 1);
      f->eax = tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
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
  struct thread *cur = thread_current();
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

pid_t
exec (const char *cmd_line)
{

}

int
wait (pid_t pid)
{

}

/*
  We first acquire a lock since this is File I/O.
  Then we call filesys_create, which creates a new file and returns true if successful.
  Lock then is released and filesys_create return value is returned.
*/
bool
create (const char *file, unsigned initial_size)
{
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
  struct file *f = (struct file *)(tof->file);
  if (f != NULL)
  {
    lock_release (&file_lock);
    return -1;
  }
  else
  {
    int file_size = (int)file_length (f);
    lock_release (&file_lock);
    return file_size;
  }
}

int
read (int fd, void *buffer, unsigned size)
{
  lock_acquire (&file_lock);
  /* Read from keyboard. */
  if (fd == 0)
  {

  }
}

int
write (int fd, const void *buffer, unsigned size)
{
  lock_acquire (&file_lock);
  /* Write to STDOUT. */
  if (fd == 1)
  {
    putbuf ((const char *)buffer, (size_t)size);
    lock_release (&file_lock);
    return size;
  }
  /* Otherwise, write to open file. */
  else
  {
    struct thread_open_file *tof = find_thread_open_file (fd);
    struct file *f = (struct file *)(tof->file);

    // TODO: Deny write to executable files.
    if (f != NULL)
    {
      int bytes_written = (int)file_write (f, buffer, (off_t)size);
      lock_release (&file_lock);
      return bytes_written;
    }
    else
    {
      lock_release (&file_lock);
      return 0;
    }
  }

  return -1;
}

void
seek (int fd, unsigned position)
{

}

unsigned
tell (int fd)
{

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
is_valid_user_vaddr (void *check_vaddr)
{
  bool valid = true;

  if (check_vaddr == NULL                 || 
      check_vaddr < (void *) 0x08048000   || 
      !is_user_vaddr (check_vaddr)        || 
      !pagedir_get_page (thread_current ()->pagedir, check_vaddr))
  {
    valid = false;
  }

  return valid;
}