#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "fcntl.h"

struct {
	struct spinlock lock;
	struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
static struct proc *swapper;

//int z=0;
int toswap = 0;
volatile int proc_count = 0;   // how many processes there are
struct spinlock proccount;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

int tmpy = 0;

void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

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
			[ZOMBIE]    "zombie",
			[KERNEL]	  "kernel"
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
		uint pges = p->sz/PGSIZE;
		if(p->swapped == 0) {
			cprintf("%d %s %d %d %s", p->pid, state, pges, p->swaps, p->name);
		}
		else {
			cprintf("%d %s %d %d/S %s", p->pid, state, pges, p->swaps, p->name);
		}
		if(p->state == SLEEPING){
			getcallerpcs((uint*)p->context->ebp+2, pc);
			for(i=0; i<10 && pc[i] != 0; i++)
				cprintf(" %p", pc[i]);
		}
		cprintf("\n");
	}
	int frper = ((double)free_pages/(double)NUMOFPAGES)*100;
	cprintf("%d%% free pages in the system\n",frper);
}


// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and return it.
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
	p->swapped = 0;
	p->swaps = 0;
	p->busyswapping = 0;
	release(&ptable.lock);

	// Allocate kernel stack if possible.
	if((p->kstack = kalloc()) == 0){
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret (see below).
	sp -= 4;
	*(uint*)sp = (uint)trapret;

	sp -= sizeof *p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint)forkret;
	acquire(&proccount);
	proc_count++;
	release(&proccount);
	return p;
}

// Set up first user process.
void
userinit(void)
{
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];

	p = allocproc();
	initproc = p;
	if(!(p->pgdir = setupkvm()))
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

	p->state = RUNNABLE;
}

void createInternalProcess(const char *name, void (*entrypoint)()) {
	char *sp;
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == UNUSED)
			goto swapfound;
	release(&ptable.lock);
	panic("no room in ptable for swapper");

	swapfound:
	p->state = EMBRYO;
	p->pid = nextpid++;
	release(&ptable.lock);

	// Allocate kernel stack if possible.
	if((p->kstack = kalloc()) == 0){
		p->state = UNUSED;
		panic("cant allocate kernel stack for swapper");
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to entrypoint (see below).
	sp -= 4;
	*(uint*)sp = (uint)entrypoint;

	sp -= sizeof *p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint)forkret;
	swapper = p;


	if(!(swapper->pgdir = setupkvm()))
		panic("swapper: out of memory?");
	//  inituvm(swapper->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
	swapper->sz = PGSIZE;
	//  memset(swapper->tf, 0, sizeof(*swapper->tf));
	//  swapper->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	//  swapper->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	//  swapper->tf->es = swapper->tf->ds;
	//  swapper->tf->ss = swapper->tf->ds;
	//  swapper->tf->eflags = FL_IF;
	//  swapper->tf->esp = PGSIZE;
	//  swapper->tf->eip = 0;
	safestrcpy(swapper->name, name, sizeof(swapper->name));
	swapper->cwd = namei("/");
	swapper->state = RUNNABLE;
	acquire(&proccount);
	proc_count++;
	release(&proccount);
	//cprintf("swapper pid: %d\n",swapper->pid);
}


// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	acquire(&ptable.lock);
	uint sz = proc->sz;
	if(n > 0){
		if(!(sz = allocuvm(proc->pgdir, sz, sz + n))) {
			release(&ptable.lock);
			return -1;
		}
	} else if(n < 0){
		if(!(sz = deallocuvm(proc->pgdir, sz, sz + n)))
			release(&ptable.lock);
		return -1;
	}
	proc->sz = sz;
	switchuvm(proc);
	release(&ptable.lock);
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

	// Allocate process.
	if((np = allocproc()) == 0)
		return -1;

	// Copy process state from p.
	//cprintf("before copyuvm of fork\n");
	acquire(&ptable.lock);
	if(!(np->pgdir = copyuvm(proc->pgdir, proc->sz))){
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		release(&ptable.lock);
		return -1;
	}
	np->sz = proc->sz;
	np->parent = proc;
	*np->tf = *proc->tf;

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for(i = 0; i < NOFILE; i++)
		if(proc->ofile[i])
			np->ofile[i] = filedup(proc->ofile[i]);
	np->cwd = idup(proc->cwd);

	pid = np->pid;
	np->state = RUNNABLE;
	safestrcpy(np->name, proc->name, sizeof(proc->name));
	release(&ptable.lock);
	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
	struct proc *p;
	int fd;

	if(proc == initproc)
		panic("init exiting");

	// Close all open files.
	for(fd = 0; fd < NOFILE; fd++){
		if(proc->ofile[fd]){
			fileclose(proc->ofile[fd]);
			proc->ofile[fd] = 0;
		}
	}

	iput(proc->cwd);
	proc->cwd = 0;

	acquire(&ptable.lock);

	// Parent might be sleeping in wait().
	wakeup1(proc->parent);

	// Pass abandoned children to init.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->parent == proc){
			p->parent = initproc;
			if(p->state == ZOMBIE)
				wakeup1(initproc);
		}
	}

	// Jump into the scheduler, never to return.
	proc->state = ZOMBIE;
	acquire(&proccount);
	proc_count--;
	release(&proccount);
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

	acquire(&ptable.lock);
	for(;;){
		// Scan through table looking for zombie children.
		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != proc)
				continue;
			havekids = 1;
			if(p->state == ZOMBIE){
				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				if(p->swapped == 0)
					freevm(p->pgdir);
				p->state = UNUSED;
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->swapped = 0;
				p->swaps = 0;
				p->busyswapping = 0;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if(!havekids || proc->killed){
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(proc, &ptable.lock);  //DOC: wait-sleep
	}
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
	struct proc *p;

	for(;;){
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire(&ptable.lock);


		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

			if((p->state != RUNNABLE) || (p->busyswapping != 0)) {
//				if(p->pid > 3)
//				cprintf("process: %d, busyswapping: %d\n",p->pid,p->busyswapping);
				continue;
			}


			if((p->swapped == 1) && (swapin == 0)) {
				//        cprintf("to swap in: %d\n",p->pid);
				swapinproc = p;
				swapin=1;
				continue;
			}
			else if(p->swapped == 1) {
				continue;
			}

			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			//      if(p->pid>3 || p->pid==2)
			//      cprintf("process: %d\n",p->pid);
			proc = p;
			switchuvm(p);
			p->state = RUNNING;
			//      if(p->pid>3)
			//      cprintf("process222: %d\n",p->pid);
			swtch(&cpu->scheduler, proc->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			proc = 0;
		}
		release(&ptable.lock);

	}
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
	int intena;

	if(!holding(&ptable.lock))
		panic("sched ptable.lock");
	if(cpu->ncli != 1)
		panic("sched locks");
	if(proc->state == RUNNING)
		panic("sched running");
	if(readeflags()&FL_IF)
		panic("sched interruptible");
	intena = cpu->intena;
	swtch(&proc->context, cpu->scheduler);
	cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
	//cprintf("in yield");
	acquire(&ptable.lock);  //DOC: yieldlock
	proc->state = RUNNABLE;
	sched();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
	if(proc == 0)
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
	proc->chan = chan;
	proc->state = SLEEPING;
	sched();

	// Tidy up.
	proc->chan = 0;

	// Reacquire original lock.
	if(lk != &ptable.lock){  //DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

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


int
sleep2(int n)
{
  uint ticks0;

  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

void
swap()
{
	int i;
	int fd = 0;
	for(;;) {
		int flee = 0;
		//yield();
		while(((((double)free_pages / (double)NUMOFPAGES)*100) <= MEM_T) && (proc_count > 4)) {
#ifdef MIN
			struct proc* np;
			uint minsz = 0xA0000;
			uint minswa = 1000000;
			swapoutproc = 0;
			acquire(&ptable.lock);
			for(np = &ptable.proc[3]; np < &ptable.proc[NPROC]; np++){
				if((np->sz <= minsz) && (np->state == RUNNABLE) && (np->swapped == 0) && ((np->sz/PGSIZE) < 18)) {
					if(np->sz < minsz || (np->sz == minsz && np->swaps < minswa)) {
					minswa = np->swaps;
					minsz = np->sz;
					swapoutproc = np;
					}
				}
			}
			if(swapoutproc == 0) {
				flee++;
				release(&ptable.lock);
				if(flee > 2)
					break;
				yield();
			}
			else {
				swapoutproc->busyswapping = 1;
				swapout = 1;
				release(&ptable.lock);
			}
#elif MAX
			struct proc* np;
			uint maxsz = 0;
			swapoutproc = 0;
			acquire(&ptable.lock);
			for(np = &ptable.proc[3]; np < &ptable.proc[NPROC]; np++){
				if((np->sz > maxsz) && (np->state == RUNNABLE) && (np->swapped == 0) && ((np->sz/PGSIZE) < 18)) {
					maxsz = np->sz;
					swapoutproc = np;
				}
			}
			if(swapoutproc == 0) {
				flee++;
				release(&ptable.lock);
				if(flee > 2)
					break;
				yield();
			}
			else {
				swapoutproc->busyswapping = 1;
				swapout = 1;
				release(&ptable.lock);
			}
#endif
			if(swapout) {
				cprintf("pid: %d , swapout\n",swapoutproc->pid);
				int pid = swapoutproc->pid;
				char str[11];
				char* ext = ".swap";
				itoa(pid,str);
				char* filename = strcat(str,ext);
				//cprintf("before open\n");
				struct file* fout = open2(filename,O_CREATE | O_RDWR,fd);
				//cprintf("after open\n");
				for(i = 0; i < (swapoutproc->sz); i+=PGSIZE){
					if(filewrite(fout, uva2ka(swapoutproc->pgdir, (void *) i), PGSIZE) < 0) {
						panic("error swapping\n");
					}
				}
				//cprintf("after writing to file");
				if(swapoutproc->pid != 0)
					freevm(swapoutproc->pgdir);
				proc->ofile[fd] = 0;
				fileclose(fout);
				acquire(&ptable.lock);
				swapoutproc->swapped = 1;
				acquire(&proccount);
				proc_count--;
				release(&proccount);
				swapoutproc->busyswapping = 0;
				swapout = 0;
				release(&ptable.lock);

				cprintf("%d finished swapping out\n",swapoutproc->pid);
			}
		}
		flee = 0;
		//sleep2(1);
		//yield();

		if(swapin) {
			cprintf("pid: %d , swapin\n",swapinproc->pid);
			int pid = swapinproc->pid;
			char str[11];
			char* ext = ".swap";
			itoa(pid,str);
			char* filename = strcat(str,ext);
			struct file* fin = open2(filename,O_RDWR,fd);

			pde_t *d = setupkvm();
			char *mem;
			if(!d) {
				panic("cannot allocate pgdir for swapping in");
			}
			for(i = 0; i < swapinproc->sz; i += PGSIZE){
				if(!(mem = kalloc())) {
					freevm(d);
					panic("cannot map pages when swapping in");
				}
				if(fileread(fin, mem, PGSIZE) < 0) {
					panic("error reading swap file");
				}
				if(!mappages(d, (void *)i, PGSIZE, PADDR(mem), PTE_W|PTE_U)) {
					freevm(d);
					panic("cannot map pages when swapping in");
				}
			}
			swapinproc->pgdir = d;
			proc->ofile[fd] = 0;
			fileclose(fin);
			if(unlink2(filename) < 0) {
				panic("cannot unlink swap file");
			}

			acquire(&ptable.lock);
			acquire(&proccount);
			proc_count++;
			release(&proccount);
			swapinproc->swaps++;
			swapinproc->swapped = 0;
			swapin = 0;
			release(&ptable.lock);

			cprintf("%d finished swapping in\n",swapinproc->pid);
		}

		yield();
	}
}


