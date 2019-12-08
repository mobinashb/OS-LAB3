#include "types.h"
#include "user.h"



int main(int argc, char* argv[])
{
    char str1[] = "inittickets";
    char str2[] = "setqueue";
    char str3[] = "setpriority";

    if(strcmp(argv[1], str1) == 0){
        inittickets(atoi(argv[2]), atoi(argv[3]));
        exit();
    }
    else if(strcmp(argv[1], str2) == 0){
        setQueue(atoi(argv[2]), atoi(argv[3]));
        exit();
    }
    else if(strcmp(argv[1], str3) == 0){
        set_priority(atoi(argv[2]), atoi(argv[3]));
        exit();
    }
    return 0;
}