#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "userprog/pagedir.h"

/* Lock for syscalls dealing with critical sections of files. */
struct lock file_lock;

static void syscall_handler (struct intr_frame *);
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
      void *file = (void *)(*((int*)f->esp + 1))
      unsigned initial_size = *((unsigned *)f->esp + 2);
      f->eax = create((const char *)file, init_size);
      break;
    }
    case SYS_REMOVE:
    {
      void *file = (void *)(*((int*)f->esp + 1))
      f->eax = remove(file);
      break;
    }
    case SYS_OPEN:
    {
      const char * file = ((const char *)args[0]);
      f->eax = open(file);
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
      int fd = args[0];
      void * buffer = (void*) args[1];
      unsigned size = (unsigned) args[2];
      f->eax = read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      /* Extract all three arguments. */
      int fd = *((int *)f->esp + 1)
      void *buffer = (void *)(*((int*)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);

      /* Call WRITE and store return in eax register. */
      f->eax = write(fd, buffer, size);

      break;
    }
    case SYS_SEEK:
    {
      int fd = args[0];
      unsigned position = args[1];
      seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      int fd = args[0];
      f->eax = tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
      int fd = args[0];
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

}

int
filesize (int fd)
{

}

int
read (int fd, void *buffer, unsigned size)
{

}

int
write (int fd, const void *buffer, unsigned size)
{
  lock_acquire (&file_lock);
  /* Write to STDOUT. */
  if (fd == 1)
  {
    putbuf ((const char *)buffer, (size_t)size)
  }
  /* Otherwise, write to open file. */
  else
  {

  }
  lock_release (&file_lock);
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