#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  set_policy(2);
  int pid1;
  int pid2;
  int pid3;
  int stats[5] = {0,0,0,0,0};
 
  set_cfs_priority(0);
  pid1 = fork();
  
  if (pid1 == 0)
  {
     for(int i = 1; i<1000000; i++)
     {
	if(i%100000 == 0)
	{
		sleep(10);
	}
      }
      get_cfs_stats(getpid(),(int*)&stats);
      printf("pid: %d\n",getpid());
      printf("cfs_priority: %d\n",stats[0]);
      printf("rtime: %d\n",stats[1]);
      printf("stime: %d\n",stats[2]);
      printf("retime: %d\n",stats[3]);
      printf("vruntime: %d\n",stats[4]);
      printf("------------------------\n\n");
      //exit(0,"set_cfs_priority(1);");
      //wait(0,0);
  }
  else 
  {
     sleep(20);
     //wait(0,0);
     set_cfs_priority(1);
     pid2 = fork();
     
     if (pid2 == 0) 
     {
     for(int i = 1; i<1000000; i++)
     {
	if(i%100000 == 0)
	{
		sleep(10);
	}
      }
      get_cfs_stats(getpid(),(int*)&stats);
      printf("pid: %d\n",getpid());
      printf("cfs_priority: %d\n",stats[0]);
      printf("rtime: %d\n",stats[1]);
      printf("stime: %d\n",stats[2]);
      printf("retime: %d\n",stats[3]);
      printf("vruntime: %d\n",stats[4]);
      printf("------------------------\n\n");
      exit(0,"set_cfs_priority(1);");
      //wait(0,0);
      }
      else
      {
         //wait(0,0);
         sleep(20);
         set_cfs_priority(2);
     pid3 = fork();
     //wait(0,0);
     if (pid3 == 0) 
     {
     for(int i = 1; i<1000000; i++)
     {
	if(i%100000 == 0)
	{
		sleep(10);
	}
      }
      get_cfs_stats(getpid(),(int*)&stats);
      printf("pid: %d\n",getpid());
      printf("cfs_priority: %d\n",stats[0]);
      printf("rtime: %d\n",stats[1]);
      printf("stime: %d\n",stats[2]);
      printf("retime: %d\n",stats[3]);
      printf("vruntime: %d\n",stats[4]);
      exit(0,"set_cfs_priority(2);");
      //wait(0,0);
      
      }
      }
      
  }    
  //wait(0,0);
  //wait(0,0);
  //wait(0,0);
  
  exit(0,"task6");
}
