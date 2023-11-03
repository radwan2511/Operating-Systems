#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  //set_policy(1);
  int a = set_ps_priority(11);
  printf("return value from function set_ps_priority(11) is: %d\n",a);
  a = set_ps_priority(0);
  printf("return value from function set_ps_priority(0) is: %d\n",a);
  a = set_ps_priority(4);
  printf("return value from function set_ps_priority(4) is: %d\n",a);
  
  exit(0,"task5");
}
