#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define EXIT_LEN 32

uint64
sys_exit(void) // task3.2
{
  int n;
  char exit_msg[EXIT_LEN];
  
  
  argint(0, &n);
  
  argstr(1, exit_msg, EXIT_LEN);
  
  
  exit(n, exit_msg);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void) // task 3.3
{
  uint64 p;
  uint64 addedP;
  
  argaddr(0, &p);
  
  
  argaddr(1, &addedP);
  
  return wait(p,addedP);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// task2
uint64
sys_memsize(void)
{
	uint size;
	size = myproc()->sz;
	return size;	  
}

// task5
uint64
sys_set_ps_priority(void)
{
  int newPriority;
  
  argint(0,&newPriority);
  
  return set_ps_priority(newPriority);
}

// task6
uint64
sys_set_cfs_priority(void)
{
  int newPriority;
  
  argint(0,&newPriority);
  
  return set_cfs_priority(newPriority);
}

// task6
uint64
sys_get_cfs_stats(void)
{
  int pid;
  uint64 statsAddr;
  argint(0, &pid);
  argaddr(1 ,&statsAddr);
  
  return get_cfs_stats(pid,statsAddr);
  //return -1;
}

uint64
sys_set_policy(void)
{
  int newPolicy;
  
  argint(0,&newPolicy);
  
  return set_policy(newPolicy);
}

