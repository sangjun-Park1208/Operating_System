#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define TIME_SILCE 10000000
#define NULL ((void*)0)

int weight = 1; // 가중치를 전역변수로 선언 -> tick마다 가중치 증가시켜서 우선순위 계산

struct {
  struct spinlock lock; // scheduling 중에 time interrupt가 걸리지 않도록 lock.
  struct proc proc[NPROC]; // 존재하는 process들의 배열
  long min_priority; // 현재 process 중, 최소 우선순위값
} ptable; // process table

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct proc* ssu_schedule(){ // 최소 우선순위값을 갖는 프로세스를 ptable에서 찾아 리턴
  struct proc* p;
  struct proc* ret = NULL;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){ // ptable을 순차적으로 탐색
    if(p->state == RUNNABLE){ // RUNNABLE 상태인 프로세스만 선택
      if(ret == NULL || ret->priority > p->priority) // ptable에 저장된 최소우선순위값과 비교, 작은 경우 갱신
        ret = p;
    }
  }

#ifdef DEBUG
  if(ret)
    cprintf("PID: %d, NAME: %s, WEIGHT: %d, PRIORITY: %d\n", ret->pid, ret->name, ret->weight, ret->priority);
#endif
  return ret; // ret값은 최소우선순위값을 갖는 프로세스 포인터
}

// 인자로 받은 프로세스의 우선순위 값 갱신
// (RUNNABLE 프로세스만을 대상으로)
void update_priority(struct proc* proc){ 
  proc->priority = proc->priority + (TIME_SILCE/proc->weight);
}

// ptable 순회를 통해 RUNNABLE한 프로세스 중 최소우선순위 값을 뽑아야 함
// 뽑은 최소우선순위값을 ptable에 저장
void update_min_priority(){
  struct proc* min = NULL;
  struct proc* p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){ // ptable 순회
    if(p->state == RUNNABLE) // RUNNABLE한 프로세스만 선택
      if(min==NULL || min->priority > p->priority) // 최소우선순위값을 갖는 프로세스를 min에 저장
        min = p;
  }

  if(min != NULL)
    ptable.min_priority = min->priority; // min의 우선순위을 ptable에 저장
}

 // 인자로 받은 proc->priority 를 ptable.min_priority로 할당
void assign_min_priority(struct proc* proc){
  proc->priority = ptable.min_priority;
}


void
pinit(void) {
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// -> caller에서 mycpu() 부를 때 interupt를 disable하게 만든 뒤에
// -> 호출되어야 한다. lapicid를 읽는 도중에 다시 스케줄링 되지 않게.
// => 정리 : 실행중인 cpu (cpus) 중에서 lapic[ID] >> 24 연산한 id값에
//          해당하는 cpu 찾아서 주소를 리턴 (다음 실행 될 cpu 찾아주는듯)
// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu* mycpu(void) {
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid(); // lapic.c 참고 -> Intercept Controller에서 할당
                      // 해 준 cpu id 리턴하는 것으로 보임
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// -> proc을 cpu 구조체에서 읽어 오는 동안 interrupt를 불가능하게 함
// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc* myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli(); // cli() : 호출되면 interrupt 발생 안 함
  c = mycpu();
  p = c->proc;
  popcli(); // sti() : 호출 이후에는 interrupt 발생 가능
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
// fork() 시 호출되는 함수. 실제 프로세스를 할당함
static struct proc* allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) // 사용하지 않는 첫 번째 프로세스룰 EMBRYO 상태로 설정
      goto found;

  release(&ptable.lock);
  return 0;

found: // 사용하지 않는 프로세스 찾으면
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->weight = weight; // 현재 전역변수 weight가중치값을 새로 찾은 p 프로세스의 weight에 할당
  weight++; // 가중치++ : 다음 스케줄링 함수 호출 시 1 증가된 값으로 new_priority 계산
  assign_min_priority(p); // 최소우선순위값으로 p의 우선순위 할당 (프로세스가 새로 생성된 경우이므로)
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
void userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  ptable.min_priority = 3; // 우선순위 시작 값 3으로 설정
  p = allocproc(); // 최소의 우선순위를 갖는 프로세스 포인터를 p에 할당
  
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
  acquire(&ptable.lock); // LOCK 걸고
  p->state = RUNNABLE; // RUNNABLE 상태로 만든 뒤에
  release(&ptable.lock); // 다시 LOCK 푼다.
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

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu(); // cpu 선택 from APIC(Advanced Programmable Interrupt Controller)
  c->proc = 0;
  
  for(;;){ // 무한루프 : 각 루프에서, 계속해서 context switching이 발생함.
    sti(); // Interrupt 발생할 수 있도록 함 
            // (아래 코드가 반복 될 시, 중간에 interrupt를 막는 cli()가 호출될 수 있으므로)

    // Loop over process table looking for process to run.
    acquire(&ptable.lock); // switching하는 중간에 interrupt가 발생하지 않도록 함.

    p = ssu_schedule(); // ssu_schedule() : 현재 RUNNABLE 프로세스 중
                        // 최소 우선순위값을 갖는 프로세스 포인터 리턴
    if(p == NULL){
      release(&ptable.lock);
      continue;
    }

    /** 가장 높은 우선순위를 갖는 프로세스로 context switching 발생 **/
    c->proc = p; // 선택한 cpu의 process가 최소 우선순위를 갖는 프로세스가 됨.
    switchuvm(p); // 메모리 내, user 공간에서의 context switching 발생
    
    p->state = RUNNING; // 프로세스 상태를 RUNNING으로 설정
    swtch(&(c->scheduler), p->context); // 첫번째 인자 -> 두번째 인자로 레지스터 단위의 주소 변화 발생
    switchkvm(); // 메모리 내, kernel 공간에서의 context switching 발생

    update_priority(p); // 선택한 프로세스의 우선순위 값 부여 (이 값이 ptable의 모든 프로세스의 우선순위값 중에 최솟값임)
    update_min_priority(); // 새로운 최소우선순위값이 생겼으므로, ptable의 min_priority값도 함께 갱신함.

    c->proc = 0;
    release(&ptable.lock); // switching 끝났으므로 interrupt 허용함.
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

  if(!holding(&ptable.lock)) // holding() : cpu가 lock을 홀딩하고 있는지 판단
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena; // (1)현재 인터럽트 가능 여부를 임시 저장
  swtch(&p->context, mycpu()->scheduler); // Context Switching (레지스터 단위)
  mycpu()->intena = intena; // 이전 상태(1)로 되돌림
}

// Give up the CPU for one scheduling round.
void yield(void) {  // RUNNING상태에서 RUNNABLE 상태로 돌아가는 과정
                    // trap.c > trap()에서 호출 됨
                    // yield()가 호출 될 때엔 myproc()->state == RUNNING 상태임

  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE; // RUNNABLE 상태로 전환
  sched(); // Context switching
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
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
static void wakeup1(void *chan){  //기존 : SLEEPING 상태의 모든 process를 RUNNABLE로 바꿈
                      //추가 : 상태전이 이후, RUNNABLE process가 달라졌으므로 다시 우선순위 계산
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      assign_min_priority(p); // 관리중인 프로세스들의 priority값 중, 가장 작은 값을 p에 부여해야 함.
    }           
  }
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
procdump(void) // trap.c > trap() -> uart.c > uartintr() 에서 사용
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

void do_weightset(int weight){ // 인자로 받은 weight 값을 현재 프로세스의 weight값으로 부여
  acquire(&ptable.lock);
  myproc()->weight = weight;
  release(&ptable.lock);
}
