
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int queue, pid;

  if(argc < 3 ){
      printf(2, "Usage: nice pid queue\n" );
      exit();
  }
  pid = atoi ( argv[1] );
  queue = atoi ( argv[2] );
  if ( queue < 0 || queue > 20 ) {
      printf(2, "Invalid queue (0-20)!\n" );
      exit();
  }
  setQueue(queue, pid);
  printprocs();
  exit();
}