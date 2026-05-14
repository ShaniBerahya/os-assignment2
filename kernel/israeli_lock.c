// Israeli lock implementation for xv6

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define NLOCKS 10
#define NQUEUE 16

struct israeli_lock {
    int used;
    int locked; // 0->unlocked, 1->locked
    int favoritism;
    struct proc *owner;
    struct proc *queue[NQUEUE];
    int queue_size;
    struct spinlock lk;
};

struct israeli_lock locks[NLOCKS];

static int rand_favoritism(int favoritism) {
    // Simulate randomness based on favoritism
    return (lcg_rand() % 100) < favoritism;
}

void israeli_lock_init(void) {
    for (int i = 0; i < NLOCKS; i++) {
        locks[i].used = 0;
        locks[i].locked = 0;
        locks[i].favoritism = 50; // Default favoritism
        locks[i].owner = 0;
        locks[i].queue_size = 0;
        initlock(&locks[i].lk, "israeli_lock");
    }
}

int israeli_create(int favoritism) {
    if (favoritism < 0 || favoritism > 100)
        return -1; // Invalid favoritism value
    // Find an unused lock and initialize it
    for (int i = 0; i < NLOCKS; i++) {
        acquire(&locks[i].lk);
        if (!locks[i].used) {
            locks[i].used = 1;
            locks[i].locked = 0;
            locks[i].favoritism = favoritism;
            locks[i].owner = 0;
            locks[i].queue_size = 0;
            release(&locks[i].lk);
            return i;
        }
        release(&locks[i].lk);
    }
    return -1; // No available lock
}

int israeli_acquire(int lock_id) {
    if (lock_id < 0 || lock_id >= NLOCKS)
        return -1; // Invalid lock ID
    struct israeli_lock *isr_lk = &locks[lock_id];

    acquire(&isr_lk->lk);
    if (!isr_lk->locked) {
        isr_lk->locked = 1;
        isr_lk->owner = myproc();
        release(&isr_lk->lk);
        return 0;
    }
    else {
        // Add current process to the queue
        if (isr_lk->queue_size < NQUEUE) {
            isr_lk->queue[isr_lk->queue_size++] = myproc();
        }
        else {
            release(&isr_lk->lk);
            return -1; // Queue is full
        }

        while (isr_lk->owner != myproc()) {
            sleep(isr_lk, &isr_lk->lk);
        }
        isr_lk->locked = 1;
        release(&isr_lk->lk);
        return 0;
    }
}

int israeli_release(int lock_id) {
    if (lock_id < 0 || lock_id >= NLOCKS)
        return -1; // Invalid lock ID

    struct israeli_lock *isr_lk = &locks[lock_id];
    int release_gid = myproc()->gid;

    acquire(&isr_lk->lk);
    if (isr_lk->owner != myproc()) {
        release(&isr_lk->lk);
        return -1; // Current process does not own the lock
    }

    if (isr_lk->queue_size == 0) {
        isr_lk->locked = 0;
        isr_lk->owner = 0;
        release(&isr_lk->lk);
        return 0;
    }

    // Wake up the next process in the queue based on favoritism
    struct proc *next_proc = 0;
    int next_index = -1;
    if (rand_favoritism(isr_lk->favoritism)){
        for (int i = 0; i < isr_lk->queue_size; i++) {
            if (isr_lk->queue[i]->gid == release_gid) {
                next_proc = isr_lk->queue[i];
                next_index = i;
                break;
            }
        }
    }

    if (!next_proc) {
        next_proc = isr_lk->queue[0];
        next_index = 0;
    }

    // Shift the queue
    for (int i = next_index; i < isr_lk->queue_size - 1; i++) {
        isr_lk->queue[i] = isr_lk->queue[i + 1];
    }
    isr_lk->queue_size--;
    isr_lk->owner = next_proc;
    wakeup(isr_lk);
    release(&isr_lk->lk);
    return 0;
}

int israeli_destroy(int lock_id) {
    if (lock_id < 0 || lock_id >= NLOCKS)
        return -1; // Invalid lock ID

    struct israeli_lock *isr_lk = &locks[lock_id];
    acquire(&isr_lk->lk);
    if (isr_lk->locked || isr_lk->queue_size > 0) {
        release(&isr_lk->lk);
        return -1; // Lock is still in use
    }
    isr_lk->used = 0;
    release(&isr_lk->lk);
    return 0;
}


