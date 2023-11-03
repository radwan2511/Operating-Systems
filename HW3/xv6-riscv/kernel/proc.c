#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

int shouldIgnore(struct proc * p)
{
  #ifdef NONE
    return 0;
  #endif
  return p->pid > 2; // not init and not shell process
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

void addToMemory(uint64 a)
{
  struct proc *p = myproc();
  if(!shouldIgnore(p))
  {
    return;
  }

  int unused_memory_index;
  struct pageInMemory *pm;
  if((unused_memory_index = unused_page_in_memory_index(p)) < 0)
  {
    int toSwap = swap_algo_page_index();
    algo_file_swapout(toSwap);
    unused_memory_index = toSwap;
  }

  pm = &p->pages_in_memory[unused_memory_index];
  pm->virtual_address = a;
  pm->used_unused = 1;

  #ifdef LAPA
  pm->age = 0xFFFFFFFF;
  #endif

  #ifndef LAPA
  pm->age = 0;
  #endif
}


// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

int writeToSwapFile_(struct proc * p)
{
  char* buff = (char*)kalloc();
  for(struct pageInSwapfile *psf = p->pages_in_swapFile; psf < &p->pages_in_swapFile[MAX_PSYC_PAGES]; psf++)
  {
    if(writeToSwapFile(p,buff, psf->offset, PGSIZE) < 0)
    {
      return -1;
    }
  }
  kfree((void*)buff);
  return 0;
}

int forkCopyFile(struct proc * parent, struct proc * child)
{
  if(!parent)
  {
    return -1;
  }
  if(!child)
  {
    return -1;
  }
  if(!parent->swapFile)
  {
    return -1;
  }
  if(!child->swapFile)
  {
    return -1;
  }

  char* buff = (char*)kalloc();
  for(struct pageInSwapfile *psf = parent->pages_in_swapFile; psf < &parent->pages_in_swapFile[MAX_PSYC_PAGES]; psf++)
  {
    if(psf->used_unused)
    {
      if(readFromSwapFile(parent, buff, psf->offset, PGSIZE) < 0)
      {
        return -1;
      }
      if(writeToSwapFile(child, buff, psf->offset, PGSIZE) < 0)
      {
        return -1;
      }
    }
  }
  kfree((void*)buff);
  return 0;
}

int createSwapFile_(struct proc *p)
{
  if(!p->swapFile && createSwapFile(p) < 0)
  {
    return -1;
  }
  int i=0;
  for(i=0; i< MAX_PSYC_PAGES; i++)
  {
    p->pages_in_swapFile[i].virtual_address = 0;
    p->pages_in_swapFile[i].offset = 0;
    p->pages_in_swapFile[i].used_unused = 0;
  }
  for(i=0; i< MAX_PSYC_PAGES; i++)
  {
    p->pages_in_memory[i].virtual_address = 0;
    p->pages_in_memory[i].age = 0;
    p->pages_in_memory[i].used_unused = 0;
  }
  p->creation_order = 0;
  return 0;
}

void removeSwapFile_(struct proc * p)
{
  int i=0;
  if(p->swapFile && removeSwapFile(p) < 0)
  {
    panic("removing swap file failed");
  }
  p->swapFile = 0;
  for(i=0; i< MAX_PSYC_PAGES; i++)
  {
    p->pages_in_swapFile[i].virtual_address = 0;
    p->pages_in_swapFile[i].offset = 0;
    p->pages_in_swapFile[i].used_unused = 0;
  }
  for(i=0; i< MAX_PSYC_PAGES; i++)
  {
    p->pages_in_memory[i].virtual_address = 0;
    p->pages_in_memory[i].age = 0;
    p->pages_in_memory[i].used_unused = 0;
  }
  p->creation_order = 0;
}

void removeMemoryPage(uint64 virtual_address)
{
  struct proc *p = myproc();
  if(!shouldIgnore(p))
  {
    return;
  }
  for(int i=0; i < MAX_PSYC_PAGES; i++)
  {
    if(p->pages_in_memory[i].virtual_address == virtual_address && p->pages_in_memory[i].used_unused)
    {
      p->pages_in_memory[i].virtual_address = 0;
      p->pages_in_memory[i].age = 0;
      p->pages_in_memory[i].used_unused = 0;
      return;
    }
  }

  for(int i=0; i < MAX_PSYC_PAGES; i++)
  {
    if(p->pages_in_swapFile[i].virtual_address == virtual_address && p->pages_in_swapFile[i].used_unused)
    {
      p->pages_in_swapFile[i].virtual_address = 0;
      p->pages_in_swapFile[i].used_unused = 0;
      return;
    }
  }
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);


  if(shouldIgnore(np))
  {
    if(createSwapFile_(np) < 0){
      freeproc(np);
      return -1;
    }
    if(writeToSwapFile_(np) < 0)
    {
      freeproc(np);
      return -1;
    }
  }

  if(shouldIgnore(p))
  {
    if(forkCopyFile(p, np) < 0){
      freeproc(np);
      removeSwapFile_(np);
      return -1;
    }
    memmove(np->pages_in_memory, p->pages_in_memory, sizeof(p->pages_in_memory));
    memmove(np->pages_in_swapFile, p->pages_in_swapFile, sizeof(p->pages_in_swapFile));
    np->creation_order = p->creation_order;
  }


  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  if(shouldIgnore(p))
  {
    removeSwapFile_(p);
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          if(shouldIgnore(pp))
          {
            removeSwapFile_(pp);
          }
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

void handle_age(struct proc * p)
{
  for(struct pageInMemory * pm = p->pages_in_memory; pm < &p->pages_in_memory[MAX_PSYC_PAGES]; pm++)
  {
    pte_t * pte;
    if( (pte = walk(p->pagetable, pm->virtual_address, 0)) == 0 )
    {
        panic("walk failed");
    }
    pm->age = (pm->age >> 1);
    if( *pte & PTE_A)
    {
      pm->age = pm->age | (1 << 31);
      *pte = *pte & ~PTE_A;
    }
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // handle age
        #ifdef NFUA
            //printf("Scheduler nfua\n");
            handle_age(p);
        #endif

        #ifdef LAPA
            //printf("Scheduler lapa\n");
            handle_age(p);
        #endif

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int unused_page_in_memory_index(struct proc * p)
{
  for(int i = 0; i< MAX_PSYC_PAGES; i++)
  {
    if(!p->pages_in_memory[i].used_unused)
    {
      return i;
    }
  }
  return -1;
}

int unused_page_in_swapFile_index()
{
  struct proc * p = myproc();
  for(int i = 0; i< MAX_PSYC_PAGES; i++)
  {
    if(!p->pages_in_swapFile[i].used_unused)
    {
      return i;
    }
  }
  return -1;
}


int swapFile_page_virtualAddress(struct proc * p, uint64 virtual_addresss)
{
  for(int i = 0; i< MAX_PSYC_PAGES; i++)
  {
    if(p->pages_in_swapFile[i].virtual_address == virtual_addresss)
    {
      return i;
    }
  }
  return -1;
}


int algo_nfua()
{
  struct proc * p = myproc();
  int index_min = 0;
  int age_min = 0xFFFFFFFF;
  int i=0;
  for(struct pageInMemory * pm = p->pages_in_memory; pm < &p->pages_in_memory[MAX_PSYC_PAGES]; pm++, i++)
  {
    if( age_min >= pm->age)
    {
      age_min = pm->age;
      index_min = i;
    }
  }
  return index_min;
}

int algo_lapa()
{
  struct proc * p = myproc();
  int index_min = 0;
  int age_min = 0xFFFFFFFF;
  int i=0;
  for(struct pageInMemory * pm = p->pages_in_memory; pm < &p->pages_in_memory[MAX_PSYC_PAGES]; pm++, i++)
  {
    // need to count ones in memory age
    int pagesInMemoryAge = pm->age;
    int countOneInMemory = 0;
    while(pagesInMemoryAge > 0)
    {
      int least_significant_bit = pagesInMemoryAge % 2;
      countOneInMemory = countOneInMemory + least_significant_bit;
      pagesInMemoryAge = pagesInMemoryAge / 2;
    }

    // need to count ones in min age
    int pagesInMemoryMinAge = 0xFFFFFFFF;
    int countOneInMinAge = 0;
    while(pagesInMemoryMinAge > 0)
    {
      int least_significant_bit = pagesInMemoryMinAge % 2;
      countOneInMinAge = countOneInMinAge + least_significant_bit;
      pagesInMemoryMinAge = pagesInMemoryMinAge / 2;
    }

    if(pagesInMemoryMinAge > countOneInMemory)
    {
      index_min = i;
      age_min = pm->age;
    }

    if(pagesInMemoryMinAge == countOneInMemory && age_min > pm->age)
    {
      index_min = i;
      age_min = pm->age;
    }
  }
  return index_min;
}

int algo_scfifo()
{
  struct proc *p = myproc();
  struct pageInMemory * pm;
  int index = p->creation_order;
  for(;;)
  {
    pm = &p->pages_in_memory[index];
    pte_t *pte;
    if((pte = walk(p->pagetable, pm->virtual_address, 0)) == 0)
    {
      panic("walk failed");
    }

    if (!(*pte & PTE_A))
    {
      p->creation_order = (index+1) % MAX_PSYC_PAGES;
      return index;
    }
    else
    {
      *pte = *pte & ~PTE_A;
      index = (index+1) % MAX_PSYC_PAGES;
    }
  }
}


int swap_algo_page_index()
{
  //printf("here\n");
  #ifdef NFUA
    //printf("NFUA\n");
    return algo_nfua();
  #endif

  #ifdef LAPA
  //printf("LAPA\n");
    return algo_lapa();
  #endif

  #ifdef SCFIFO
  //printf("SCFIFO\n");
    return algo_scfifo();
  #endif

  #ifdef NONE
    return -1;
  #endif

  return -1;
}

void algo_file_swapout(int page_memory_index)
{
  struct proc *p = myproc();
  if(page_memory_index < 0 || page_memory_index >= MAX_PSYC_PAGES)
  {
    panic("reached max memory pages");
  }
  struct pageInMemory * pm = &p->pages_in_memory[page_memory_index];

  if(!pm->used_unused)
  {
    panic("page unused");
  }

  pte_t * pte;
  if( ( pte = walk(p->pagetable, pm->virtual_address, 0)) == 0)
  {
    panic("walk failed");
  }
  if(!(*pte & PTE_V) || (*pte & PTE_PG))
  {
    panic("page not in memory");
  }

  int page_swapFile_index;
  if((page_swapFile_index = unused_page_in_swapFile_index()) < 0)
  {
    panic("reached max swapFile pages");
  }

  struct pageInSwapfile * psf = &p->pages_in_swapFile[page_swapFile_index];
  uint64 physicalAddress = PTE2PA(*pte);
  if(writeToSwapFile(p,(char*)physicalAddress, psf->offset, PGSIZE) < 0 )
  {
    panic("writeToSwapFile failed");
  }
  psf->virtual_address = pm->virtual_address;
  psf->used_unused = 1;
  kfree((void*)physicalAddress);

  pm->used_unused = 0;
  pm->virtual_address = 0;

  *pte = *pte & ~PTE_V;
  // to secondary storage
  *pte = *pte | PTE_PG;
  /// flush the TLB.
  sfence_vma();
}

void algo_file_swapin(int swapFile_index, int memory_index)
{
  if(swapFile_index < 0 || swapFile_index >= MAX_PSYC_PAGES)
  {
    panic("reached max swap file pages");
  }

  if(memory_index < 0 || memory_index >= MAX_PSYC_PAGES)
  {
    panic("reached max memory pages");
  }

  struct proc * p = myproc();
  struct pageInSwapfile *  psf = &p->pages_in_swapFile[swapFile_index];

  if(!psf->used_unused)
  {
    panic("page in swap file is unused");
  }

  pte_t *pte;
  if((pte = walk(p->pagetable, psf->virtual_address,0)) == 0)
  {
    panic("not valid pte (unallocated)");
  }
  if((*pte & PTE_V) || !(*pte & PTE_PG))
  {
    panic("page not in swap file");
  }

  struct pageInMemory * pm = &p->pages_in_memory[memory_index];
  if(pm->used_unused)
  {
    panic("page in memory is used");
  }

  uint64 buff;
  if ( (buff = (uint64)kalloc() ) == 0 )
  {
    panic("memory allocation failed");
  }

  if(readFromSwapFile(p, (char*)buff, psf->offset, PGSIZE) < 0)
  {
    panic("read from file failed");
  }

  
  pm->virtual_address = psf->virtual_address;
  pm->used_unused = 1;

  #ifdef LAPA
  pm->age = 0xFFFFFFFF;
  #endif

  #ifndef LAPA
  pm->age = 0;
  #endif

  psf->used_unused = 0;
  psf->virtual_address = 0;
  
  *pte = *pte | PTE_V;
  *pte = *pte & ~PTE_PG;
  // using buff , update pte 
  *pte = PA2PTE(buff) | PTE_FLAGS(*pte);
  // flush TLB
  sfence_vma();

}

void dealWithPageFault(uint64 virtual_addresss)
{
   struct proc *p = myproc();
   pte_t * pte;
   if(!(pte=walk(p->pagetable, virtual_addresss, 0)))
   {
    panic("walk failed");
   }

   if(*pte & PTE_V)
   {
    panic("not valid pte");
   }

   if(!(*pte & PTE_PG))
   {
    panic("PTE_PG");
   }


  int pages_memory_unused;
  if( (pages_memory_unused = unused_page_in_memory_index(p)) < 0)
  {
      int memory_to_swap = swap_algo_page_index();
      algo_file_swapout(memory_to_swap);
      pages_memory_unused = memory_to_swap;
  }
  int index;
  if ( (index = swapFile_page_virtualAddress(p, PGROUNDDOWN(virtual_addresss))) < 0 )
  {
    panic("page in swap file index fault");
  }

  algo_file_swapin(index, pages_memory_unused);
}