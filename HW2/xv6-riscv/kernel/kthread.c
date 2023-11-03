#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];


void kthreadinit(struct proc *p)
{
  initlock(&p->counter_lock, "nextkid");
  
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&(kt->klock), "klock");
    kt->kstate = KUNUSED;
    kt->kproc = p;
    
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

// Return the current struct kthread *, or zero if none.
struct kthread *mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *t = 0;
  if ( c->kthread != 0)
      t = c->kthread;
  
  pop_off();
  return t;
}

int
allockid(struct proc *p)
{
  int kid;
  
  acquire(&p->counter_lock);
  kid = p->counter;
  p->counter = p->counter + 1;
  release(&p->counter_lock);

  return kid;
}

// static 
struct kthread*
allockthread(struct proc* p, uint64 kforkret)
{
  struct kthread *kt;
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
    acquire(&kt->klock);
    if(kt->kstate == KUNUSED) {
      goto found;
    } else {
      release(&kt->klock);
    }
  }
  return 0;

found:
  kt->kid = allockid(p);
  kt->kstate = KUSED;
  
  if ( (kt->trapframe = get_kthread_trapframe(p, kt)) == 0)
  {
      freekthread(kt);
      release(&kt->klock);
      return 0;
  }
    
  
  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&kt->kcontext, 0, sizeof(kt->kcontext));
  kt->kcontext.ra = kforkret;
  kt->kcontext.sp = kt->kstack + PGSIZE;
  
  return kt;
}


struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

 
void
freekthread(struct kthread *kt)
{
  kt->trapframe = 0;
  
  kt->kid = 0;
  kt->kchan = 0;
  kt->kkilled = 0;
  kt->kxstate = 0;
  kt->kstate = KUNUSED;
}




