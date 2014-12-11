#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>



int run_debugger();



int main(int argc, char *argv[])
{
  int i;

  printf("starting debugger\n");
  run_debugger();

  for (i=0; i<10; i++) {
    printf("tick %i\n", 10-i);
    sleep(1);
  }
  printf("done\n");
  return 0;
}
