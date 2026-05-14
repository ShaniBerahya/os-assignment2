// user/ilocktest.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 12
#define GROUPS 3

void
run_competition(int favoritism)
{
    printf("\n");
    printf("====================================\n");
    printf("TEST START favoritism=%d\n", favoritism);
    printf("====================================\n");

    int lock = israeli_create(favoritism);

    if(lock < 0){
        printf("FAILED: create lock\n");
        exit(1);
    }

    lcg_srand(12345);

    for(int i = 0; i < NPROCS; i++){

        int pid = fork();

        if(pid == 0){

            //
            // each child gets a group
            //
            int gid = lcg_rand() % GROUPS;

            setgid(gid);

            //
            // try to acquire lock
            //
            if(israeli_acquire(lock) < 0){
                printf("[ERROR] pid=%d acquire failed\n",
                       getpid());
                exit(1);
            }

            //
            // IMPORTANT:
            // print ONLY while holding the lock
            // so output stays ordered
            //
            printf("[ACQUIRE] pid=%d gid=%d favoritism=%d\n",
                   getpid(),
                   getgid(),
                   favoritism);

            //
            // keep lock for a bit
            //
            sleep(20);

            printf("[RELEASE] pid=%d gid=%d\n",
                   getpid(),
                   getgid());

            if(israeli_release(lock) < 0){
                printf("[ERROR] pid=%d release failed\n",
                       getpid());
                exit(1);
            }

            exit(0);
        }
    }

    //
    // wait all children
    //
    for(int i = 0; i < NPROCS; i++)
        wait(0);

    if(israeli_destroy(lock) < 0){
        printf("FAILED: destroy lock\n");
        exit(1);
    }

    printf("TEST END favoritism=%d\n", favoritism);
}

void
invalid_id_test(void)
{
    printf("\n");
    printf("====================================\n");
    printf("INVALID LOCK ID TEST\n");
    printf("====================================\n");

    if(israeli_acquire(-1) < 0)
        printf("[PASS] acquire(-1) rejected\n");
    else
        printf("[FAIL] acquire(-1) accepted\n");

    if(israeli_release(9999) < 0)
        printf("[PASS] release(9999) rejected\n");
    else
        printf("[FAIL] release(9999) accepted\n");
}

void
non_owner_release_test(void)
{
    printf("\n");
    printf("====================================\n");
    printf("NON OWNER RELEASE TEST\n");
    printf("====================================\n");

    int lock = israeli_create(50);

    if(lock < 0){
        printf("FAILED: create lock\n");
        exit(1);
    }

    int pid = fork();

    if(pid == 0){

        setgid(1);

        israeli_acquire(lock);

        printf("[CHILD OWNER] pid=%d gid=%d acquired lock\n",
               getpid(),
               getgid());

        sleep(100);

        israeli_release(lock);

        exit(0);
    }

    //
    // let child acquire first
    //
    sleep(20);

    //
    // parent tries illegal release
    //
    if(israeli_release(lock) < 0)
        printf("[PASS] non-owner release rejected\n");
    else
        printf("[FAIL] non-owner release succeeded\n");

    wait(0);

    israeli_destroy(lock);
}

void
destroy_active_test(void)
{
    printf("\n");
    printf("====================================\n");
    printf("DESTROY ACTIVE LOCK TEST\n");
    printf("====================================\n");

    int lock = israeli_create(50);

    israeli_acquire(lock);

    if(israeli_destroy(lock) < 0)
        printf("[PASS] destroy active lock rejected\n");
    else
        printf("[FAIL] destroy active lock succeeded\n");

    israeli_release(lock);

    israeli_destroy(lock);
}

void
gid_test(void)
{
    printf("\n");
    printf("====================================\n");
    printf("GID TEST\n");
    printf("====================================\n");

    setgid(7);

    int gid = getgid();

    if(gid == 7)
        printf("[PASS] gid correctly set to %d\n", gid);
    else
        printf("[FAIL] expected gid=7 got gid=%d\n", gid);
}

int
main(void)
{
    printf("\n");
    printf("####################################\n");
    printf("ISRAELI LOCK TEST SUITE START\n");
    printf("####################################\n");

    gid_test();

    invalid_id_test();

    non_owner_release_test();

    destroy_active_test();

    //
    // favoritism experiments
    //
    run_competition(0);

    run_competition(50);

    run_competition(100);

    printf("\n");
    printf("####################################\n");
    printf("ALL TESTS FINISHED\n");
    printf("####################################\n");

    exit(0);
}