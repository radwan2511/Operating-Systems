//#include "kernel/types.h"
#//include "kernel/stat.h"
//#include "user/user.h"
#include "user/ustack.h"

int
main(int argc, char *argv[])
{
  int* int_ptr = (int*)ustack_malloc(sizeof(int));;
  
  *int_ptr = 42;
  char* char_ptr = (char*)ustack_malloc(10*sizeof(char));
  char_ptr[0] = 'H';
  char_ptr[1] = 'e';
  char_ptr[2] = 'l';
  char_ptr[3] = 'l';
  char_ptr[4] = 'o';
  char_ptr[5] = '\0';
  

  printf("char: %s (supposed to be 'Hello')\n",char_ptr);
  printf("int:%d (supposed to be 42)\n", *int_ptr);
  printf("ustack_free return value:%d (should be 4)\n", ustack_free());
  printf("ustack_free return value:%d (should be 10)\n", ustack_free());
  printf("ustack_free return value:%d (should be -1)\n", ustack_free());
  
  exit(0);
}
