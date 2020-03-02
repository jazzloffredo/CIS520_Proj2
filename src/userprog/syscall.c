#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
s_halt (void)
{
  shutdown_power_off ();
}

static void
s_exit (void)
{

}
