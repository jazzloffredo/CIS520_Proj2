#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"

/* Lock for syscalls dealing with critical sections of files. */
struct lock file_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *system_call = f->esp;
  int args[4];

 //Get the args into array format for evaluation in switch statement
  for(int i = 0; i < 4; i++) {
    args[i] = *((int*) f->esp+i);
  }

  /*Switch case to check what Syscall was made and calls the appropriate function by passing in needed args, 
    if the function returns anything but void it is stored on the return register (eax)*/
  switch (*system_call)
  {
    case SYS_HALT:
    {
      halt ();
      break;
    }
    case SYS_EXIT:
    {
      int status = args[0];
      exit(status);
      break;
    }
    case SYS_EXEC:
    {
      const char * cmd_line = ((const char *)args[0]);
      f->eax = exec(cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      int p_id = args[0];
      f->eax = wait(p_id);
      break;
    }
    case SYS_CREATE:
    {
      const char * file = ((const char *)args[0]);
      unsigned init_size = (unsigned) args[1];
      f->eax = create(file, init_size);
      break;
    }
    case SYS_REMOVE:
    {
      const char * file = ((const char *)args[0]);
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
      int fd = args[0];
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
      int fd = args[0];
      const void * buffer = (const void*) args[1];
      unsigned size = (unsigned) args[2];
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

/* Prints to console the current threads name and status when exiting based off what was passed in
   and then exits via thread_exit(). */
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