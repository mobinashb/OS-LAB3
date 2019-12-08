#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h" 

int main(int argc, char const *argv[])
{
    set_priority(0, getpid());
    int pid = getpid();
    int i;

    for(i = 1; i < 7; i++)
    {
        pid = fork();
        if(pid > 0)
           set_priority(i, pid);
        else
            break;
    }

    if(pid < 0)
    {
        printf(2, "fork error\n");
    }
    else if(pid == 0)
    {
        int child_pid = getpid();
        printf(1, "Child Process %d started \n", child_pid);
        sleepsec(2);
        printf(1, "Child Process %d finished\n---------------------- \n", child_pid);
    }
    else
    {
        int i;
        for(i = 0; i < 7; i++)
            wait();
        printf(1, "Parent Process %d finished\n", pid);
    }
    return 0;
}