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
                //do nothing
            }
            if(pid == 0 )
                break;
            
        }
    }
       
    if(pid == 0)
    {
        int i;
        int j = 0;
        for(i = 0 ; i < 200000000000 ; i++)
        {
            j = j+1;
        }
    }
    else
    {
        int i;
        for(i = 0; i < 10; i++)
            wait();
    }
  exit();
}