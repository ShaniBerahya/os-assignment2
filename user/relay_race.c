#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROC 15
#define TARGET_SCORE 30
#define MAX_GROUPS 8

struct race_result {
    int favoritism;
    int groups;
    int winner;
    int scores[MAX_GROUPS];
};
struct race_result run_race(int favoritism, int groups) {
    struct race_result result;
    result.favoritism = favoritism;
    result.groups = groups;
    result.winner = -1;

    int lock_id = israeli_create(favoritism);

    if(lock_id < 0){
        printf("FAILED: create lock\n");
        exit(1);
    }

    if(team_score_init(lock_id, TARGET_SCORE) < 0){
        printf("FAILED: initialize team scores\n");
        exit(1);
    }

    lcg_srand(1234 + favoritism + groups);

    for(int i = 0; i < NPROC; i++){
        int pid = fork();

        if(pid == 0){
            // child process - runner
            int gid = i % groups; // Assign group ID based on process index (no rendomness)

            setgid(gid);

            while(1){
                // Check if race is finished before trying to acquire lock
                if(is_race_finished(lock_id)){
                    exit(0);
                }

                if(israeli_acquire(lock_id) < 0){
                    printf("acquire failed\n");
                    exit(1);
                }

                int score = team_score_update(lock_id, gid);

                if(score <= 0){
                    // did not update score due to error or race finished
                    israeli_release(lock_id);
                    exit(0);
                }

                printf("[pid=%d gid=%d] TEAM %d SCORE=%d\n",
                       getpid(), gid, gid, score);

                if(score >= TARGET_SCORE){
                    printf("\n*** TEAM %d WINS ***\n", gid);
                    result.winner = gid;

                    israeli_release(lock_id);
                    exit(0);
                }

                israeli_release(lock_id);

                sleep(1);
            }
        }
    }

    for(int i = 0; i < NPROC; i++)
        wait(0);

    printf("\nFINAL SCORES:\n");

    for(int g = 0; g < groups; g++){
        int score = team_score_get(lock_id, g);
        result.scores[g] = score;
        printf("TEAM %d -> %d\n", g, score);
    }

    israeli_destroy(lock_id);

    printf("RACE ENDED favoritism=%d groups=%d\n",
           favoritism, groups);

    return result;
}

int main(void) {
    struct race_result results[9];
    int result_count = 0;

    printf("\n=== RELAY RACE TOURNAMENT ===\n");
    printf("\n=== 2 GROUPS ===\n");
    results[result_count++] = run_race(0, 2);
    results[result_count++] = run_race(50, 2);
    results[result_count++] = run_race(100, 2);

    printf("\n=== 3 GROUPS ===\n");
    results[result_count++] = run_race(0, 3);
    results[result_count++] = run_race(50, 3);
    results[result_count++] = run_race(100, 3);

    printf("\n=== 5 GROUPS ===\n");
    results[result_count++] = run_race(0, 5);
    results[result_count++] = run_race(50, 5);
    results[result_count++] = run_race(100, 5);

    printf("\n=== ALL RACES FINISHED ===\n");

    // Print summary results
    printf("\n=== RACE RESULTS SUMMARY ===\n");
    for(int i = 0; i < result_count; i++){
        struct race_result r = results[i];
        printf("Favoritism: %d, Groups: %d, Winner: TEAM %d | Scores: ", r.favoritism, r.groups, r.winner);

        for(int g = 0; g < r.groups; g++){
            printf("TEAM %d: %d", g, r.scores[g]);
            if(g < r.groups - 1) printf(", ");
        }
        printf("\n");
    }

    exit(0);
}