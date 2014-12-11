#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define error(msg) do { \
  fprintf(stderr, "%s:%d: '%s' %s\n", __FILE__, __LINE__, msg, strerror(errno)); \
  fflush(stderr); \
  } while(0) 

#ifdef __x86_64__

void print_regs(pid_t pid)
{
  struct user_regs_struct regs;
  unsigned long long instr=0;

  if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
    error("ptrace");
    return;
  }

  instr = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, 0);

  printf("rip %016llx (%016llx)\n", regs.rip, instr);
}

#else

void print_regs(pid_t pid)
{
  struct user_regs_struct regs;
  unsigned instr=0;

  if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
    error("ptrace");
    return;
  }

  instr = ptrace(PTRACE_PEEKTEXT, pid, regs.eip, 0);

  printf("eip %08x (%08x)\n", regs.eip, instr);

}

#endif




static void debugger(pid_t pid) 
{
  int wait_status;
  unsigned icounter = 0;


  printf("attaching to = %d\n", pid);
  fflush(stdout);

  if (ptrace(PTRACE_SEIZE, pid, 0, 0) < 0) {
    error("ptrace");
    return ;
  }
 
  if (ptrace(PTRACE_INTERRUPT, pid, 0, 0) < 0) {
    error("ptrace");
    return ;
  }

  waitpid(pid, &wait_status, 0);
  while (WIFSTOPPED(wait_status)) { 
    icounter++;
    print_regs(pid);
    /* 
     * make proc continue. 
     */
    if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0) < 0) {
      error("ptrace");
      return;
    }
    
    waitpid(pid, &wait_status, 0);
  }

  printf("icounter = %d\n", icounter);

}


int run_debugger()
{
  pid_t parent = getpid();
  printf("my pid = %d\n", parent);
  fflush(stdout);

  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
  
  prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

  pid_t p = fork();
  if (p == 0) {
    /* child proc, i.e., the debugger. */
    assert(parent == getppid());
    debugger(parent);
    exit(1);
  }
  else if (p > 0) {
    /* parent. p = child proc. */



    return 0;
  }

  error("fork");
  return -1;
}


