#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

  if(argc <= 1)
  {
    exit(-1,"task7Error : no argument given");
  }
  
  int newPolicy = argv[1][0] - '0'; // convert char to int
  set_policy(newPolicy);
  
  exit(0,"task7Success");
}
