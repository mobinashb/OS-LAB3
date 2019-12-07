#include "types.h"
#include "user.h"


void HRRNScheduling() {
    int pid = getpid();
    if (pid > 0) {
        sleepsec(3);
        pid = fork();
        
    }

    if (pid < 0)
    {
        printf(2, "fork error\n");
    }

    else if (pid == 0)
    {
        sleepsec(4);
        printf(1, "pid %d exited\n", getpid());
        exit();
    }

    else
    {
        sleepsec(1);
        pid = fork();
        if (pid > 0) {
            printprocs();
            wait();
            wait();
            wait();
            printf(1, "pid %d exited\n", getpid());
            exit();
        }
        else {
            sleepsec(2);
            printf(1, "pid %d exited\n", getpid());
            exit();
        }
    }
}

int main(int argc, char* argv[])
{
    HRRNScheduling();
    wait();
    return 0;
}