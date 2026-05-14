#include "types.h"
#include "defs.h"
#include "spinlock.h"

static uint lcg_state;
static struct spinlock lcg_lock;

void
lcg_init(void)
{
  initlock(&lcg_lock, "lcg");
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
  acquire(&lcg_lock);
  lcg_state = a * lcg_state + b;
  uint result = lcg_state;
  release(&lcg_lock);
  return result;
}
