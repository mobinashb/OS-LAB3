#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf(1, "sleep seconds not entered!\n");
        exit();
    }
    else {
        int seconds = atoi(argv[1]);
        gettime();
        int sleptFor = sleepsec(seconds);
        gettime();
        printf(1, "slept for: %d seconds\n", sleptFor);
        exit();
    }
}