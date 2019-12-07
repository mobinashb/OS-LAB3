#include "types.h"
#include "user.h" // for using from strlen



void lotteryScheduling() {
    int pid = getpid();
    if (pid > 0) {
        // sleepsec(3);
        pid = fork();
        
    }

    if (pid < 0)
    {
        printf(2, "fork error\n");
    }

    else if (pid == 0)
    {
        printf(1, "pid %d exited\n", getpid());
        int mypid = getpid();
        inittickets(12187, mypid);
        // sleepsec(4);
        
        exit();
    }

    else
    {
        // sleepsec(1);
        pid = fork();
        if (pid > 0) {
            pid = fork();
            if (pid > 0) {
                int mypid = getpid();
                inittickets(110, mypid);
                sleepsec(2);
                printprocs();
                wait();
                wait();
                wait();
                
                printf(1, "pid %d exited\n", getpid());
                exit();
            }
            else {
                int mypid = getpid();
                inittickets(108, mypid);
                // sleepsec(2);
                printf(1, "pid %d exited\n", getpid());
                exit();
            }
            
        }
        else {
            int mypid = getpid();
            inittickets(100, mypid);
            // sleepsec(2);
            printf(1, "pid %d exited\n", getpid());
            exit();
        }
    }
}

int main(int argc, char* argv[])
{
    lotteryScheduling();
    wait();
    return 0;
}