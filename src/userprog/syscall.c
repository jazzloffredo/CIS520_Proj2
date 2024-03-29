#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
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
static struct lock file_lock;

/* Lock for syscalls dealing with reading/writing from system. */
static struct lock sys_lock;

static void syscall_handler (struct intr_frame *);
static struct thread_open_file *find_thread_open_file (int);
static inline bool is_page_mapped (void *);
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
      check_valid_buffer (cmd_line, sizeof(cmd_line));
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

/* Terminates Pintos by calling shutdown_power_off() 
   (declared in threads/init.h). This should be seldom 
   used, because you lose some information about possible 
   deadlock situa- tions, etc. */
void
halt (void)
{
  shutdown_power_off ();
}

/* 
  Terminates the current user program, returning status 
  to the kernel. If the process’s parent waits for it 
  (see below), this is the status that will be returned. 
  Conventionally, a status of 0 indicates success and 
  nonzero values indicate errors.
  
  This implementation inspired by:
  https://github.com/MohamedSamirShabaan/Pintos-Project-2
*/
void
exit (int status)
{
  struct thread *cur = thread_current();

  /* Print error status for tests. */
  printf ("%s: exit(%d)\n", cur->name, status);

  /* Grab the thread_child from parents children list. */
  struct thread_child *c = thread_get_child (&cur->parent->children, cur->tid);

  /* About to exit, update status. */
  c->exit_status = status;

  thread_exit();
}

/* Runs the executable whose name is given in cmd_line,
   passing any given arguments, and returns the new 
   process’s program ID (pid). Must return pid -1, which 
   otherwise should not be a valid pid, if the program 
   cannot load or run for any reason. Thus, the parent 
   process cannot return from the exec until it knows 
   whether the child process successfully loaded its executable. 
   You must use appropriate synchronization to ensure this. */
pid_t
exec (const char *cmd_line)
{
  struct thread *parent = thread_current();

  /* Execute the new process. */
  pid_t pid = process_execute(cmd_line);

  /* Get the child thread after it has either completely or partially executed. */
  struct thread_child *c = thread_get_child(&parent->children, pid);

  /* Put parent to sleep while child attempts to load. */
  sema_down(&c->child_thread->load_sema);

  /* Return -1 if child did not load correctly. Otherwise, just return PID from execution. */
  if(!c->load_success)
    return -1;

  return pid;
}

/*
  Waits for a child process pid and retrieves the child’s exit status.

  If pid is still alive, waits until it terminates. Then, returns 
  the status that pid passed to exit. If pid did not call exit(), 
  but was terminated by the kernel (e.g. killed due to an exception), 
  wait(pid) must return -1. It is perfectly legal for a parent process 
  to wait for child pro- cesses that have already terminated by the 
  time the parent calls wait, but the kernel must still allow the parent 
  to retrieve its child’s exit status, or learn that the child was 
  terminated by the kernel.

  This implementation inspired by:
  https://github.com/MohamedSamirShabaan/Pintos-Project-2
*/
int
wait (pid_t pid)
{
  return process_wait (pid);
}

/* Creates a new file called file initially initial_size bytes in size. 
   Returns true if successful, false otherwise. Creating a new file does 
   not open it: opening the new file is a separate operation which would 
   require a open system call. */
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

/* Deletes the file called file. Returns true if successful, 
   false otherwise. A file may be removed regardless of whether 
   it is open or closed, and removing an open file does not close it. */
bool
remove (const char *file)
{
  lock_acquire(&file_lock);

    bool remove_successful = filesys_remove(file);

  lock_release(&file_lock);

  return remove_successful;
}

/* Opens the file called file. Returns a nonnegative 
   integer handle called a "file descriptor" (fd), 
   or -1 if the file could not be opened. */
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
    list_push_back(&thread_current ()->open_files, &new_file->elem);
    thread_current ()->fd_counter += 1;

    lock_release (&file_lock);
    return cur_fd;
  }
}

/* Returns the size, in bytes, of the file open as fd. */
int
filesize (int fd)
{
  lock_acquire (&file_lock);

    struct thread_open_file *tof = find_thread_open_file (fd);
    if (tof == NULL)
      return -1;
    struct file *f = tof->file;
    int file_size = file_length (f);
  
  lock_release (&file_lock);

  return file_size;
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of 
   file), or -1 if the file could not be read (due to a 
   condition other than end of file). Fd 0 reads from the 
   keyboard using input_getc(). */
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
    struct file *f = tof->file;
    int bytes_read = file_read (f, buffer, size);
  
  lock_release (&file_lock);

  return bytes_read;
}

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually writ- ten, 
   which may be less than size if some bytes could 
   not be written. */
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
    
    int bytes_written = 0;
    bytes_written = (int)file_write (tof->file, buffer, (off_t)size);

  lock_release (&file_lock);
  return bytes_written;
}

/* Changes the next byte to be read or written in open file 
   fd to position, expressed in bytes from the beginning of 
   the file. (Thus, a position of 0 is the file’s start.) */
void
seek (int fd, unsigned position)
{
  lock_acquire (&file_lock);
  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    struct file *f = tof->file;
    file_seek (f, position);
  }
  lock_release (&file_lock);
}

/* Returns the position of the next byte to be read 
   or written in open file fd, expressed in bytes 
   from the beginning of the file. */
unsigned
tell (int fd)
{
  lock_acquire (&file_lock);
  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    struct file *f = tof->file;
    unsigned position = file_tell (f);
    lock_release (&file_lock);
    return position;
  }

  lock_release (&file_lock);
  return -1;
}

/* Closes file descriptor fd. Exiting or terminating a 
   process implicitly closes all its open file descriptors, 
   as if by calling this function for each one. */
void
close (int fd)
{
  lock_acquire (&file_lock);

  struct thread_open_file *tof = find_thread_open_file (fd);
  if (tof != NULL)
  {
    file_close (tof->file);
    list_remove (&tof->elem);
    free (tof);
  }
  
  lock_release (&file_lock);
}

/* Finds an open file in the current threads open_files list. */
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

/* Checks if a virtual address is mapped to user memory. */
static inline bool
is_page_mapped (void *check_vaddr)
{
  return pagedir_get_page (thread_current ()->pagedir, check_vaddr) == NULL ? false: true;
}

/* Exits with error status if vaddr is invalid. */
static void
check_valid_user_vaddr (const void *check_vaddr)
{
  if (check_vaddr == NULL                 || 
      check_vaddr < (void *)0x08048000    || 
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