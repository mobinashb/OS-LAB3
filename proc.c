#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->queue = 1;
  

  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  p->arrivalTime = xticks;
  p->execCycles = 1;
  p->tickets = 50;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  uint xticks;
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  p->state = RUNNABLE;

  //HRRN
  p->arrivalTime = xticks;
  p->execCycles = 1;
  p->priority = 0.0;

  //SRPF
  p->priority = 0;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  np->queue = 1;
  np->tickets = 10;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int intSize(int i){
    if (i >= 1000000000)
      return 10;
    if (i >= 100000000)
      return 9;
    if (i >= 10000000)
      return 8;
    if (i >= 1000000)
      return 7;
    if (i >= 100000)
      return 6;
    if (i >= 10000)
      return 5;
    if (i >= 1000)
      return 4;
    if (i >= 100)
      return 3;
    if (i >= 10)
      return 2;
    return 1;
}

char* print_state(int state){
  if(state == 0){
    return "UNUSED";
  }else if(state == 1){
    return "EMBRYO";
  }else if(state == 2){
    return "SLEEPING";
  }else if(state == 3){
    return "RUNNABLE";
  }else if(state == 4){
    return "RUNNING";
  }else if(state == 5){
    return "ZOMBIE";
  }else{
    return "";
  }
}

void
printProcesses()
{
  struct proc *p;
  int siz = 0;
  int i = 0 ;
  char* state;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid != 0 && siz < strlen(p->name))
      siz = strlen(p->name);
  }
  cprintf("name");
  for(i = 0 ; i < siz - strlen("name") + 3 ; i++)
    cprintf(" ");
  cprintf("pid");
  for(i = 0 ; i < 4; i++)
    cprintf(" ");
  cprintf("state");
  for(i = 0 ; i < 6; i++)
    cprintf(" ");
  cprintf("execCycles");
  for(i = 0 ; i < 3; i++)
    cprintf(" ");
  cprintf("arrivalTime");
  for(i = 0 ; i < 3; i++)
    cprintf(" ");
  cprintf("tickets");
  for(i = 0 ; i < 3; i++)
    cprintf(" ");
  cprintf("queue");
  for(i = 0 ; i < 3; i++)
    cprintf(" ");
  cprintf("priority");
  cprintf("\n________________________________________________________________________________ \n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == 0)
      continue;
    cprintf("%s", p->name);
    for(i = 0 ; i < siz - strlen(p->name) + 4 ; i++)
      cprintf(" ");
    cprintf("%d", p->pid);
    for(i = 0 ; i < 6 - intSize(p->pid); i++)
      cprintf(" ");
    state = print_state(p->state);
    cprintf("%s" , state);
    for(i = 0 ; i < 11 - strlen(state); i++)
      cprintf(" ");
    cprintf("%d  ", p->execCycles);
    for(i = 0 ; i < 11 - intSize(p->execCycles); i++)
      cprintf(" ");
    cprintf("%d  ", p->arrivalTime);
    for(i = 0 ; i <  12 - intSize(p->arrivalTime); i++)
      cprintf(" ");
    cprintf("%d  ", p->tickets);
    for(i = 0 ; i < 8 - intSize(p->tickets); i++)
      cprintf(" ");
    cprintf("%d  ", p->queue);
    for(i = 0 ; i < 6 - intSize(p->queue); i++)
      cprintf(" ");
    if(p->priority - (int)(p->priority) > 0.001)
      cprintf("%d", (int)(p->priority) + 1);
    else if(p->priority - (int)(p->priority) < 0.001)
      cprintf("%d", (int)(p->priority));
    cprintf("\n");
  }
}

int totalTickets(void)
{
  struct proc *p;
  int numOfTickets = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE && p->queue == 1){
      numOfTickets += p->tickets;
    }
  }
  return numOfTickets;
}

uint generateRandomNumber(uint max)
{
  uint t = ticks;
  uint ranNum = 123456789 + t*t*t*t*t - (100 + t);
  return (ranNum) % max;
}

void find_and_set_lottery_ticket(uint lotteryTickets, int pid){
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (pid == p->pid) {
      p -> tickets = lotteryTickets;
      break;
    }
  }
}

void find_and_set_SRPF_priority(int priority, int pid)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(pid == p->pid)
    {
      p->priority = (float)priority;
      break;
    }
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

void
changeQueue(int queue, int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(pid == p->pid ) {
        p->queue = queue;
        break;
    }
  }
  release(&ptable.lock);
}

struct proc*
HRRNScheduler(void)
{
  struct proc *p;
  struct proc *highestHRRN = 0;
  float maxHRRN = 0;
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE && p->queue == 2) {
          if(highestHRRN == 0)
            highestHRRN = p;
          else {
            uint xticks;
            acquire(&tickslock);
            xticks = ticks;
            release(&tickslock);
            uint waitingTime = xticks - p->arrivalTime;
            float HRRN = (int)waitingTime / (int)p->execCycles;
            uint waitingTime2 = xticks - highestHRRN->arrivalTime;
            maxHRRN = (int)waitingTime2 / (int)highestHRRN->execCycles;
            if (HRRN > maxHRRN)
              highestHRRN = p;
            
          }
      }
  }
  
  return highestHRRN;
}

struct proc* 
lotteryScheduler(void)
{
  uint counter = 0;
  uint numOfTickets = totalTickets();
  uint randomTicket = generateRandomNumber(numOfTickets + 1);
  struct proc *p;
  struct proc *luckyProcess = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state != RUNNABLE || p->queue != 1)
      continue;
    if ((counter + p->tickets) < randomTicket){
      counter += p->tickets;
      continue;
    }
    if (luckyProcess != 0) {
      return luckyProcess;
    }
    else {
      luckyProcess = p;
    }
  }
  return luckyProcess;
}

struct proc*
SRPFScheduler(void)
{
  uint t = ticks;
  int selected = 0;
  struct proc *p;
  struct proc *selected_proc = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state != RUNNABLE || p->queue != 3) 
      continue;
    else if(!selected) {
      selected_proc = p;
      selected = 1;
    }

    if(selected_proc->priority > p->priority ) 
      selected_proc = p;
    else if (selected_proc->priority == p->priority)
      if (t % 2 == 0)
        selected_proc = p;
  }
  return selected_proc;
}

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    p = lotteryScheduler();
    if (p == 0)
      p = HRRNScheduler(); 
    if (p == 0)
      p = SRPFScheduler();
    
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    if (p != 0) {
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      //HRRN
      p->execCycles++;

      //SRPF
      if (p->priority > 0 && p->queue == 3)
        p->priority -= 0.1;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
