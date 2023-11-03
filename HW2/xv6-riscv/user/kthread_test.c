#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/syscall.h"

#define MAX_STACK_SIZE1 4000

void kthread_start_func(void){
  for(int i=0; i<10; i++){
    sleep(10); // simulate work
    printf("Sleeping...\n");
  }
  kthread_exit(0);
  printf("kthread_exit failed\n");
  exit(1);
}

void
main(int argc, char *argv[])
{
  uint64 stack_a = (uint64)malloc(MAX_STACK_SIZE1);
  uint64 stack_b = (uint64)malloc(MAX_STACK_SIZE1);

  int kt_a = kthread_create((void *(*)())kthread_start_func, stack_a, MAX_STACK_SIZE1);
  if(kt_a <= 0){
    printf("kthread_create failed\n");
    exit(1);
  }
  printf("Create A successfull\n");
  int kt_b = kthread_create((void *(*)())kthread_start_func, stack_b, MAX_STACK_SIZE1);
  if(kt_a <= 0){
    printf("kthread_create failed\n");
    exit(1);
  }

  printf("Create B successfull\n");
  int joined = kthread_join(kt_a, 0);
  if(joined != 0){
    printf("kthread_join failed\n");
    exit(1);
  }

  joined = kthread_join(kt_b, 0);
  if(joined != 0){
    printf("kthread_join failed\n");
    exit(1);
  }


  printf("Join successfull\n");
  free((void *)stack_a);
  free((void *)stack_b);

printf("everything successfull\n");

}

