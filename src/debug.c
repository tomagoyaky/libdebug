#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>

#define error(msg) do { \
    fprintf(stderr, "%s:%d: '%s' %s\n", __FILE__, __LINE__, msg, strerror(errno)); \
    fflush(stderr); \
  } while(0) 


#define trace(...) do { \
    printf(__VA_ARGS__); \
    fflush(stdout); \
  } while(0)



#define MAX_MSG_LEN 1024
static int debugger_write_pipe = -1;
static int debugger_read_pipe  = -1;


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



int debugger_command(char *msg, char *reply, int *len, int maxlen)
{
  char buf[MAX_MSG_LEN];
  int msglen = strlen(msg)+1;
  int replen;

  assert(msglen <= MAX_MSG_LEN);

  write(debugger_write_pipe, msg, msglen);

  replen = read(debugger_read_pipe, reply, maxlen);
  reply[replen] = 0;
  if (len)
    *len = replen;

  return replen;
}


int dbg_watch(void *addr)
{
  char buf[MAX_MSG_LEN];
  char rep[MAX_MSG_LEN];

  sprintf(buf, "watch %p", addr);

  debugger_command(buf, rep, NULL, MAX_MSG_LEN);
  trace("sent: '%s' reply '%s'\n", buf, rep);
}


int dbg_break(void *addr)
{
  char buf[MAX_MSG_LEN];
  char rep[MAX_MSG_LEN];

  sprintf(buf, "break %p", addr);

  debugger_command(buf, rep, NULL, MAX_MSG_LEN);
  trace("sent: '%s' reply '%s'\n", buf, rep);
}


static void debugger(pid_t pid) 
{
  int wait_status;
  unsigned icounter = 0;
  int singlestepping = 0;
  int cnt;
  fd_set rfds;
  struct timeval tv;
  int retval;

  trace("attaching to = %d\n", pid);

  if (ptrace(PTRACE_SEIZE, pid, 0, 0) < 0) {
    error("ptrace");
    return ;
  }
 
  if (ptrace(PTRACE_INTERRUPT, pid, 0, 0) < 0) {
    error("ptrace");
    return ;
  }

  waitpid(pid, &wait_status, 0);

  /* 
   * main select loop for debugger proc.
   */
  while (1) {
    trace("mydbg %d\n", icounter);
    icounter++;

    FD_ZERO(&rfds);
    FD_SET(debugger_read_pipe, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    cnt = select(debugger_read_pipe+1, &rfds, NULL, NULL, &tv);

    if (cnt > 0) {
      char msg[MAX_MSG_LEN];
      char *reply;


      if (read(debugger_read_pipe, msg, MAX_MSG_LEN) == -1) {
          
      }

      trace("mydbg '%s'\n", msg);
      /* handle commnand. */
      char *cmd = strtok(msg, " ");

      if (strcmp(cmd, "hello")==0) {
        reply = "OK";
      }
      else if (strcmp(cmd, "watch")==0) {
        unsigned long dr7;
        char *saddr = strtok(NULL, "");
        void *addr;
        int n = 0;
        sscanf(saddr, "%p", &addr);

        ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[0]), addr);
       
        dr7 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[7]), 0);
        dr7 |= 3 << (24+2*n);
        dr7 |= 3 << (16+2*n);
        dr7 |= 3 << (2*n);
        dr7 = ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[7]), dr7);
        reply = "OK";

      }
      else if (strcmp(cmd, "break")==0) {
        unsigned long dr7;
        char *saddr = strtok(NULL, "");
        void *addr;
        int n = 1;

        sscanf(saddr, "%p", &addr);

        ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[1]), addr);

        dr7 = ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[7]), 0);
        
        dr7 |= 3 << (24+2*n);
        dr7 &= ~(3 << (16+2*n));
        dr7 |= 3 << (2*n);

        dr7 = ptrace(PTRACE_POKEUSER, pid, offsetof(struct user, u_debugreg[7]), dr7);

        reply = "OK";
      }
      else if (strcmp(cmd, "continue")==0) {

        reply = "OK";
      }
      else if (strcmp(cmd, "clear")==0) {

        reply = "OK";
      }
      else {
        reply = "ERROR";
      }

      write(debugger_write_pipe, reply, strlen(reply)+1);
    }
  
    if (WIFSTOPPED(wait_status)) { 
      //print_regs(pid);
      
      /* 
      * make proc continue. 
      */
      if (singlestepping) {
        if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0) < 0) {
          error("ptrace");
          continue;
        }
      }
      else {
        if (ptrace(PTRACE_CONT, pid, 0, 0) < 0) {
          error("ptrace");
          continue;
        }
      }
      //waitpid(pid, &wait_status, WNOHANG);
    }
  }

}


int run_debugger()
{
  int pipe1[2];
  int pipe2[2];
  pid_t parent = getpid();
  printf("my pid = %d\n", parent);
  fflush(stdout);

  /* set self to be traceable/debuggable. */
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
  
  prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);


  if (pipe(pipe1) < 0) {
    error("pipe");
    return -1;
  }

  if (pipe(pipe2) < 0) {
    error("pipe");
    return -1;
  }

  pid_t p = fork();
  if (p == 0) {
    /* child proc, i.e., the debugger. */
    assert(parent == getppid());

    /* assign pipe id's and close unused ends*/
    debugger_write_pipe = pipe1[1];
    debugger_read_pipe = pipe2[0];
    close(pipe1[0]);
    close(pipe2[1]);



    debugger(parent);
    exit(1);
  }
  else if (p > 0) {
    char reply[MAX_MSG_LEN];
    int  replen;
    /* parent. p = child proc. */

    /* assign pipe id's and close unused ends*/
    debugger_write_pipe = pipe2[1];
    debugger_read_pipe = pipe1[0];
    close(pipe1[1]);
    close(pipe2[0]);


    /* Send a message to child and wait for reply. */
    
    
    debugger_command("hello", reply, &replen, MAX_MSG_LEN);
    printf("got message '%s' back\n", reply);

    return 0;
  }

  error("fork");
  return -1;
}


