#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>



int run_debugger();
int dbg_watch(void *addr);



void foo()
{

  printf("foo!\n");

}


int main(int argc, char *argv[])
{
  int i, j, k = 0;

  printf("starting debugger\n");
  run_debugger();

#if 0
  for (i=0; i<10; i++) {
    printf("tick %i\n", 10-i);
    sleep(1);
  }
#endif

  dbg_watch(&k);
  
  dbg_break(foo);

  for (i=0; i<100; i++) {
    j = j + i;
    if (i == 42) 
      k = 42;
  }

  sleep(1);
  k = 0;
  printf("k = %d\n", k);

  sleep(1);
  printf("done\n");
  return 0;
}
