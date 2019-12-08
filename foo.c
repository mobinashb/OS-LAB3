#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main()
{
  int pid = getpid();
    
    int i;
    for(i = 1; i < 5; i++)
    {
        if(pid > 0)
        {
            pid = fork();
            if(pid > 0)
            {
            // set_sched_queue(PRIORITY, pid);
            // set_priority(10-i, pid);
            }
            if(pid == 0 )
                break;
            
        }
    }
       
    if(pid < 0)
    {
        printf(2, "fork error\n");
    }
    else if(pid == 0)
    {
        int i;
        int j = 0;
        for(i = 0 ; i < 200000000000 ; i++)
        {
            j = j+1;
        }
        
        //printf(1, "%d\n", ownPid);

    }
    else
    {
        int i;
        for(i = 0; i < 10; i++)
            wait();
        printf(1, "Main user program finished pid %d\n", getpid());
    }
  exit();
}