#include "types.h"
#include "user.h" // for using from strlen


void lotteryScheduling() {
    int pid = getpid();
    if (pid > 0) {
        pid = fork();
        
    }

    if (pid < 0)
    {
        printf(2, "fork error\n");
    }

    else if (pid == 0)
    {
        int mypid = getpid();
        inittickets(12, mypid);
        sleepsec(2);
        printf(1, "pid %d exited ** ", getpid());
        exit();
    }

    else
    {
        pid = fork();
        if (pid > 0) {
            pid = fork();
            if (pid > 0) {
                int mypid = getpid();
                inittickets(110, mypid);
                printprocs();
                wait();
                wait();
                wait();
                wait();
                printf(1, "pid %d exited ** ", getpid());
                exit();
            }
            else {
                int mypid = getpid();
                inittickets(10000, mypid);
                sleepsec(2);
                printf(1, "pid %d exited ** ", getpid());
                exit();
            }
            
        }
        else {
            int mypid = getpid();
            inittickets(100, mypid);
            sleepsec(2);
            printf(1, "pid %d exited ** ", getpid());
            exit();
        }
    }
}

int main(int argc, char* argv[])
{
    lotteryScheduling();
    sleepsec(3);
    wait();
    exit();
    return 0;
}