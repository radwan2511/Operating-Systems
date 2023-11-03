#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  uint a = memsize();
  printf("bytes of memory the running process is using: %d\n",a);
  printf("alocating 20k more bytes of memory by calling malloc() ...\n");
  void *b = malloc(20000);
  a = memsize();
  printf("bytes of memory the running process is using after the allocation: %d\n",a);
  printf("freeing the allocated array ...\n");
  free(b);
  a = memsize();
  printf("bytes of memory the running process is using after the release: %d\n",a);  
  exit(0,"task2");
}
