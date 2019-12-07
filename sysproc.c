#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_gettime(void)
{
  struct rtcdate r;
  cmostime(&r);
  cprintf("current time: %d:%d:%d\n", (&r)->hour, (&r)->minute, (&r)->second);
  return r.second;
}

int
sys_sleepsec(void)
{
  uint xticks = 0;
  int secs;
  uint startTime;
  uint finishTime;
  if (argint(0, &secs) < 0) {
    return -1;
  }
  startTime = sys_uptime();
  while (((int)xticks - (int)startTime) < secs*100)
  {
    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
  }
  finishTime = sys_uptime();
  return ((int)finishTime-(int)startTime)/100;
}

void 
sys_printprocs(void)
{
  printProcesses();
}

void
sys_inittickets(void)
{
  int lottery_ticket;
  argint(0, &lottery_ticket);
  int pid;
  argint(1, &pid);
  find_and_set_lottery_ticket((uint)lottery_ticket , pid);
}

void 
sys_set_priority()
{
  int priority;
  argint(0, &priority);
  int pid;
  argint(1, &pid);
  find_and_set_SRPF_priority(priority, pid);
}