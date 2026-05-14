// Israeli lock implementation for xv6

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define NLOCKS 15
#define NQUEUE 16
#define NGROUPS 8


/// Israeli lock ///
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


/// Race - Team score management ///
// This structure and functions manage team scores based on the Israeli lock ownership
struct race {
    int scores[NGROUPS];
    int isr_lk_id; // Associated Israeli lock ID
    int finished;
    int winner;
    int target_score;
};

static struct race race;

int team_score_init(int isr_lk_id, int target_score) {
    if (isr_lk_id < 0 || isr_lk_id >= NLOCKS)
        return -1 ; // Invalid lock ID
    race.isr_lk_id = isr_lk_id;
    race.finished = 0;
    race.winner = -1;
    race.target_score = target_score;
    for (int j = 0; j < NGROUPS; j++) {
        race.scores[j] = 0;
    }
    return 0;
}

// returns updated score or -1 on error or 0 if race finished
int team_score_update(int lock_id, int group_id) {
    if (lock_id < 0 || lock_id >= NLOCKS || group_id < 0 || group_id >= NGROUPS)
        return -1; // Invalid ID

    struct proc *me = myproc();

    // Check if current process owns the Israeli lock
    acquire(&locks[lock_id].lk);
    if (locks[lock_id].owner != me || lock_id != race.isr_lk_id) {
        release(&locks[lock_id].lk);
        return -1; // Current process does not own the lock
    }

    if (race.finished) {
        release(&locks[lock_id].lk);
        return 0; //   Race already finished
    }
    race.scores[group_id]++;
    if (race.scores[group_id] >= race.target_score) {
        race.finished = 1;
        race.winner = group_id;
    }
    int score = race.scores[group_id];
    release(&locks[lock_id].lk);

    return score;
}

int team_score_get(int lock_id, int group_id) {
    if (lock_id < 0 || lock_id >= NLOCKS || group_id < 0 || group_id >= NGROUPS)
        return -1; // Invalid group ID

    if (lock_id != race.isr_lk_id)
        return -1; // Wrong race lock ID

    acquire(&locks[lock_id].lk);
    int score = race.scores[group_id];
    release(&locks[lock_id].lk);

    return score;
}

int is_race_finished(int lock_id) {
    if (lock_id < 0 || lock_id >= NLOCKS)
        return -1; // Invalid lock ID

    if (lock_id != race.isr_lk_id)
        return -1; // Wrong race lock ID

    acquire(&locks[lock_id].lk);
    int finished = race.finished;
    release(&locks[lock_id].lk);
    return finished;
}
