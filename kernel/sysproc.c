#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

static uint lcg_state;
static struct spinlock lcg_lock;

void
lcg_init(void)
{
  initlock(&lcg_lock, "lcg");
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void
lcg_srand(uint seed)
{
  acquire(&lcg_lock);
  lcg_state = seed;
  release(&lcg_lock);
}

uint
lcg_rand(void)
{
  uint a = 1664525;
  uint b = 1013904223;
  uint m = 0xFFFFFFFF; // 2^32 - 1
  acquire(&lcg_lock);
  lcg_state = (a * lcg_state + b) % (m + 1);
  uint result = lcg_state;
  release(&lcg_lock);
  return result;
}

uint64
sys_lcg_srand(void)
{
  int seed;
  argint(0, &seed);
  lcg_srand(seed);
  return 0;
}

uint64
sys_lcg_rand(void)
{
  return lcg_rand();
}
