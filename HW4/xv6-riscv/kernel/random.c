#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"


struct {
  struct spinlock lock;
  uint8 random_seed;
}random_dev;

// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
uint8 lfsr_char(uint8 lfsr)
{
uint8 bit;
bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
lfsr = (lfsr >> 1) | (bit << 7);
return lfsr;
}
int randomwrite(int fd, const uint64 src, int n) {
  if (n != 1)
    return -1;
  uint8 seed;
  acquire(&random_dev.lock);
  if (copyin(myproc()->pagetable, (char*)&seed,src, 1) < 0){
    return -1;
  }
  random_dev.random_seed = seed;
  release(&random_dev.lock);

  return 1;
}

int randomread(int fd, uint64 dst, int n)
{ 

  int bytes_written = 0;
  acquire(&random_dev.lock);
  while (n > 0) {
    uint8 seed = lfsr_char(random_dev.random_seed); 
    if (copyout(myproc()->pagetable, dst, (char*)&seed, 1) < 0)
      break;
    dst++;  
    n--;
    bytes_written++;
    random_dev.random_seed = seed;
  }
  release(&random_dev.lock);

  return bytes_written;
}


void
randominit(void)
{
  initlock(&random_dev.lock, "random");
  random_dev.random_seed = 0x2A;
  devsw[RANDOM].read = randomread;
  devsw[RANDOM].write = randomwrite;
  
}


