#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "filesys/filesys.h"

//Lock for File SysCall Critical Sections
struct lock file_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  //Might Need Lock Here (see slides)
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *system_call = f->esp;

  switch (*system_call)
  {
    case SYS_HALT:
      halt ();
  }
}

static void
halt (void)
{
  shutdown_power_off ();
}

static void
exit (int status)
{
  
}

static pid_t
exec (const char *cmd_line)
{

}

static int
wait (pid_t pid)
{

}

/*
  We first acquire a lock since this is File I/O.
  Then we call filesys_create, which creates a new file and returns true if successful.
  Lock then is released and filesys_create return value is returned.
*/
static bool
create (const char *file, unsigned initial_size)
{
  lock_aquire(&file_lock);
  bool retVal = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return retVal;
}

/*
  We acquire a lock first since this is File I/O.
  Then we call filesys_remove, which removes a file given a filename and returns true if successful.
  Lock is then released and the return value of filesys_remove is returned.
*/
static bool
remove (const char *file)
{
  lock_aquire(&file_lock);
  bool retVal = filesys_remove(file);
  lock_release(&file_lock);
  return retVal;
}

static int
open (const char *file)
{

}

static int
filesize (int fd)
{

}

static int
read (int fd, void *buffer, unsigned size)
{

}

static int
write (int fd, const void *buffer, unsigned size)
{

}

static void
seek (int fd, unsigned position)
{

}

static unsigned
tell (int fd)
{

}

static void
close (int fd)
{
  
}