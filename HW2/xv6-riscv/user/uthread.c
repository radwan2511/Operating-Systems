#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uthread.h"

// task1.1
struct uthread* currThread;
struct uthread allProcessThreads[MAX_UTHREADS];
int systemThreads = 0;

enum sched_priority priority_helper; // used to find max priority

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
	// find free thread in the array
	struct uthread* th;
	int i;
	for( i=0 ;i< MAX_UTHREADS && allProcessThreads[i].state != FREE; i++);
	
	if(allProcessThreads[i].state != FREE)
		return -1; //failure

	th = &allProcessThreads[i];
	// Set up new context to start executing, same as in allocproc function in proc.c
  	// which returns to user space.
	memset(&th->context, 0, sizeof(th->context));
	th->priority = priority;
	th->state = RUNNABLE;
	th->context.ra = (uint64)start_func;
	th->context.sp = (uint64)&(th->ustack) + STACK_SIZE;
	systemThreads += 1;
	return 0; //success
}


void uthread_yield()
{
	// same as yield function in proc.c
	currThread->state = RUNNABLE;
	
	struct uthread* t;
	struct uthread* nextT = allProcessThreads;
	priority_helper = LOW;
	for( t = allProcessThreads; t < &allProcessThreads[MAX_UTHREADS]; t++)
	{
		if(t->state == RUNNABLE &&  priority_helper <= t->priority)
		{
			priority_helper = t->priority;
			nextT = t;
		}
	}
	t = currThread;
	currThread = nextT;
	currThread->state = RUNNING;
	uswtch(&t->context, &nextT->context); // function in uthread.h
}

void uthread_exit()
{
	currThread->state = FREE;
	currThread->priority = 0;
	systemThreads -= 1;
	if(systemThreads <= 0)
		exit(0);

	struct uthread* t;
	struct uthread* nextT = allProcessThreads;
	priority_helper = LOW;
	for( t = allProcessThreads; t < &allProcessThreads[MAX_UTHREADS]; t++)
	{
		if(t->state == RUNNABLE && priority_helper <= t->priority)
		{
			priority_helper = t->priority;
			nextT = t;
		}
	}
	t = currThread;
	currThread = nextT;
	currThread->state = RUNNING;
	uswtch(&t->context, &currThread->context); // function in uthread.h
}


int uthread_start_all()
{
	// same as in forkret in proc.c
	static int first = 1;
	if (first) {

		first = 0;
		
		struct uthread* t;
		struct uthread* nextT = allProcessThreads;
		priority_helper = LOW;
		for( t = allProcessThreads; t < &allProcessThreads[MAX_UTHREADS]; t++)
		{
			if(t->state == RUNNABLE &&  priority_helper <= t->priority)
			{
				priority_helper = t->priority;
				nextT = t;
			}
		} 
		t = currThread;
		currThread = nextT;
		currThread->state = RUNNING;
		
		//uswtch(&context, &nextT->context); // function in uthread.h
		
		((void (*)())(currThread->context.ra))();
		exit(0);
  	}
	else
	{
		return -1;
	}
  	
}

enum sched_priority uthread_set_priority(enum sched_priority priority)
{
	enum sched_priority returnValue = currThread->priority;
	currThread->priority = priority;
	return returnValue;
}


enum sched_priority uthread_get_priority()
{
	return currThread->priority;
}

struct uthread* uthread_self()
{
	return currThread;
}


