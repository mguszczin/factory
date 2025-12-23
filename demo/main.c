#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <math.h>

// Adjust paths to match your project structure
#include "../common/err.h"
#include "../common/plant.h"

/* -------------------------------------------------------------------------- */
/*                                Test Helpers                                */
/* -------------------------------------------------------------------------- */

#define GREEN "\033[0;32m"
#define RED   "\033[0;31m"
#define RESET "\033[0m"

#define TEST_PASS() printf(GREEN "PASS\n" RESET)
#define TEST_FAIL(msg) do { printf(RED "FAIL: %s (Line %d)\n" RESET, msg, __LINE__); return -1; } while(0)

// Helper to allocate result memory (int*)
void setup_task_memory(task_t* t, int capacity) {
    t->results = (int*)malloc(sizeof(int) * capacity);
}

void cleanup_task_memory(task_t* t) {
    if (t->results) free(t->results);
}

// Simple work function: returns 1
int work_fn_simple(worker_t* worker, task_t* task, int aux) {
    usleep(10000); // 10ms work
    return 1;
}

/* -------------------------------------------------------------------------- */
/*                                 Scenarios                                  */
/* -------------------------------------------------------------------------- */

/**
 * Scenario 1: The "Zombie" Worker (Your specific example)
 * 
 * Condition: Worker is available [Now, Now+1].
 *            Task starts at [Now+3].
 * 
 * Expected: Task cannot be started because by the time it reaches start time,
 *           the worker has gone home.
 *           collect_task should return ERROR (task skipped).
 */
int test_worker_ends_before_task_starts() {
    printf("Test 1: Worker ends shift before task starts... ");
    fflush(stdout);

    int stations[] = {1};
    if (init_plant(stations, 1, 1) != PLANTOK) TEST_FAIL("Init failed");

    time_t now = time(NULL);

    // Worker works for only 1 second
    worker_t w = { .id = 1, .start = now, .end = now + 1, .work = work_fn_simple };
    add_worker(&w);

    // Task starts in 3 seconds
    task_t t = { .id = 100, .start = now + 3, .capacity = 1 };
    setup_task_memory(&t, 1);
    add_task(&t);

    // Should return ERROR because task is skipped (impossible to schedule)
    int res = collect_task(&t);
    cleanup_task_memory(&t);
    destroy_plant();

    if (res == ERROR) {
        TEST_PASS();
        return 0;
    } else {
        TEST_FAIL("Task should have been skipped (returned ERROR), but returned PLANTOK");
    }
}

/**
 * Scenario 2: The "Patient" Destroyer
 * 
 * Condition: Task starts in the future (T+2).
 *            We call destroy_plant() immediately at T+0.
 * 
 * Expected: destroy_plant() MUST NOT exit immediately. 
 *           It must wait for the task to become active, execute, and finish.
 *           Execution time should be approx 2 seconds.
 */
int test_destroy_waits_for_future() {
    printf("Test 2: Destroy waits for future task... ");
    fflush(stdout);

    int stations[] = {1};
    init_plant(stations, 1, 1);

    time_t now = time(NULL);

    // Worker is available for a long time
    worker_t w = { .id = 1, .start = now, .end = now + 10, .work = work_fn_simple };
    add_worker(&w);

    // Task starts in 2 seconds
    task_t t = { .id = 200, .start = now + 2, .capacity = 1 };
    setup_task_memory(&t, 1);
    add_task(&t);
    // We do NOT call collect_task. We go straight to destroy.
    // destroy_plant should see the pending task and wait.
    time_t start = time(NULL);
    collect_task(&t);
    destroy_plant();
    time_t end = time(NULL);

    cleanup_task_memory(&t);

    double elapsed = difftime(end, start);
    if (elapsed >= 2.0) {
        TEST_PASS();
        return 0;
    } else {
        printf("(Elapsed: %.1fs) ", elapsed);
        TEST_FAIL("destroy_plant exited too early! It abandoned the future task.");
    }
}

/**
 * Scenario 3: The "Late Arriving" Worker
 * 
 * Condition: Task starts Now.
 *            Worker starts at Now+2.
 * 
 * Expected: Task sits in PENDING state until worker arrives.
 *           Total collection time approx 2 seconds.
 *           Verifies Alarm thread wakes up on worker->start.
 */
int test_worker_starts_late() {
    printf("Test 3: Worker starts late (Alarm on Worker Start)... ");
    fflush(stdout);

    int stations[] = {1};
    init_plant(stations, 1, 1);

    time_t now = time(NULL);

    // Worker starts in 2 seconds
    worker_t w = { .id = 1, .start = now + 2, .end = now + 10, .work = work_fn_simple };
    add_worker(&w);

    // Task wants to start NOW
    task_t t = { .id = 300, .start = now, .capacity = 1 };
    setup_task_memory(&t, 1);
    add_task(&t);

    time_t start = time(NULL);
    if (collect_task(&t) != PLANTOK) TEST_FAIL("Collect failed");
    time_t end = time(NULL);

    cleanup_task_memory(&t);
    destroy_plant();

    double elapsed = difftime(end, start);
    if (elapsed >= 2.0) {
        TEST_PASS();
        return 0;
    } else {
        TEST_FAIL("Task finished too early! Did not wait for worker start time?");
    }
}

/**
 * Scenario 4: Multiple Future Tasks + Destroy
 * 
 * Condition: Task A starts T+1. Task B starts T+3.
 *            Call destroy_plant() at T+0.
 * 
 * Expected: Destroy waits ~3 seconds total. Both tasks execute.
 */
int test_destroy_waits_multiple_future() {
    printf("Test 4: Destroy waits for sequence of future tasks... ");
    fflush(stdout);

    int stations[] = {2};
    init_plant(stations, 1, 2);

    time_t now = time(NULL);
    worker_t w1 = { .id = 1, .start = now, .end = now + 20, .work = work_fn_simple };
    worker_t w2 = { .id = 2, .start = now, .end = now + 20, .work = work_fn_simple };
    add_worker(&w1);
    add_worker(&w2);

    task_t t1 = { .id = 401, .start = now + 1, .capacity = 1 };
    task_t t2 = { .id = 402, .start = now + 3, .capacity = 1 };
    setup_task_memory(&t1, 1);
    setup_task_memory(&t2, 1);

    add_task(&t1);
    add_task(&t2);

    time_t start = time(NULL);
    collect_task(&t1);
    collect_task(&t2);
    destroy_plant();
    time_t end = time(NULL);

    // Clean up memory manually since we didn't collect
    cleanup_task_memory(&t1);
    cleanup_task_memory(&t2);

    double elapsed = difftime(end, start);
    // Task 2 finishes at T+3 + epsilon.
    if (elapsed >= 3.0) {
        TEST_PASS();
        return 0;
    } else {
        printf("(Elapsed: %.1fs) ", elapsed);
        TEST_FAIL("destroy_plant didn't wait for the second future task.");
    }
}

/**
 * Scenario 5: Shift Boundary Edge Case
 * 
 * Condition: Worker ends at T+2. Task starts at T+2.
 *            (Depending on implementation using > vs >=, this might succeed or fail).
 *            Standard logic: if start == end, strictly no time to work.
 *            But if start < end, 1 second of overlap is enough?
 * 
 * Let's test: Worker [Now, Now+2]. Task [Now+1].
 * Duration 1 second. Should Pass.
 */
int test_worker_tight_schedule() {
    printf("Test 5: Worker tight schedule (valid overlap)... ");
    fflush(stdout);

    int stations[] = {1};
    init_plant(stations, 1, 1);

    time_t now = time(NULL);

    // Worker available for 2 seconds
    worker_t w = { .id = 1, .start = now, .end = now + 2, .work = work_fn_simple };
    add_worker(&w);

    // Task starts at +1 second.
    // Worker has 1 second remaining (from T+1 to T+2). Should be enough.
    task_t t = { .id = 500, .start = now + 1, .capacity = 1 };
    setup_task_memory(&t, 1);
    add_task(&t);

    int res = collect_task(&t);

    cleanup_task_memory(&t);
    destroy_plant();

    if (res == PLANTOK) {
        TEST_PASS();
        return 0;
    } else {
        TEST_FAIL("Task skipped despite valid time window overlap");
    }
}

/**
 * Scenario 6: Mixed Immediate and Future with Destroy
 * 
 * Condition: T1 (Immediate), T2 (Future).
 *            Destroy called immediately.
 *            T1 should finish fast, T2 waits.
 */
int test_mixed_destroy() {
    printf("Test 6: Mixed Immediate & Future with Destroy... ");
    fflush(stdout);

    int stations[] = {2};
    init_plant(stations, 1, 2);
    time_t now = time(NULL);

    worker_t w1 = { .id = 1, .start = now, .end = now + 10, .work = work_fn_simple };
    add_worker(&w1);

    // T1: Immediate
    task_t t1 = { .id = 601, .start = now, .capacity = 1 };
    setup_task_memory(&t1, 1);
    add_task(&t1);

    // T2: Future (T+2)
    task_t t2 = { .id = 602, .start = now + 2, .capacity = 1 };
    setup_task_memory(&t2, 1);
    add_task(&t2);

    time_t start = time(NULL);
    collect_task(&t2);
    collect_task(&t1);
    destroy_plant();
    time_t end = time(NULL);

    cleanup_task_memory(&t1);
    cleanup_task_memory(&t2);

    if (difftime(end, start) >= 2.0) {
        TEST_PASS();
        return 0;
    } else {
        TEST_FAIL("Destroy exited before future task T2 ran");
    }
}

/**
 * Helper: Work function that sleeps for 1 second.
 */
int work_sleep_1s(worker_t* w, task_t* t, int i) {
    usleep(1000000); // 1 sekunda
    return 1;
}

/**
 * Scenario 7: Massive Parallelism
 * 
 * Condition: 10 Workers. 1 Task with capacity 10.
 *            Station has capacity 10.
 *            Each 'slot' of the task takes 1 second.
 * 
 * Expected: All 10 threads launch approx at the same time.
 *           Total wall-clock time should be ~1s.
 *           If it takes ~10s, parallelism is broken.
 */
int test_massive_parallelism() {
    printf("Test 7: Massive Parallelism (10 threads at once)... ");
    fflush(stdout);

    // 1. Init: Stacja musi być duża, żeby przyjąć zadanie o capacity 10
    int stations[] = {10}; 
    if (init_plant(stations, 1, 10) != PLANTOK) TEST_FAIL("Init failed");

    time_t now = time(NULL);

    // 2. Dodajemy 10 pracowników
    worker_t workers[10];
    for(int i=0; i<10; ++i) {
        workers[i].id = i;
        workers[i].start = now;
        workers[i].end = now + 20; // Długa szychta
        workers[i].work = work_sleep_1s;
        add_worker(&workers[i]);
    }

    // 3. Dodajemy 1 zadanie wymagające 10 pracowników naraz
    task_t t = { .id = 777, .start = now, .capacity = 10 };
    setup_task_memory(&t, 10);
    add_task(&t);

    // 4. Mierzymy czas wykonania collect_task
    time_t start = time(NULL);
    int res = collect_task(&t);
    time_t end = time(NULL);

    cleanup_task_memory(&t);
    destroy_plant();

    if (res != PLANTOK) {
        TEST_FAIL("Collect task returned error");
    }

    double elapsed = difftime(end, start);
    
    // Debug info
    // printf("(Elapsed: %.1fs) ", elapsed);

    // Jeśli trwało mniej niż 3 sekundy (dajemy margines), to jest OK.
    // Sekwencyjnie trwałoby 10s.
    if (elapsed < 3.0) {
        TEST_PASS();
        return 0;
    } else {
        printf("[Time: %.1fs] ", elapsed);
        TEST_FAIL("Execution was too slow! Looks sequential instead of parallel.");
    }
}

static int n_workers = 20;
static int n_stations = 10;
int stations[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

typedef struct {
    pthread_barrier_t* barrier;
    unsigned int seed;
    int id;
} thread_args_t;

typedef struct {
    int id;
    time_t start_time;
    int* status_ptr;
    pthread_barrier_t* barrier;
} and_and_collect_t;

typedef struct {
    task_t* t;
    worker_t* w;
    pthread_barrier_t* barier;
    int* status_ptr;
    unsigned int seed;
} worker_and_task_t;

void* client_thread_func(void* arg) {
    worker_and_task_t* info = (worker_and_task_t*)arg;
    
    setup_task_memory(info->t, 1); 
    int res;

    pthread_barrier_wait(info->barier);

    add_worker(info->w);
    add_task(info->t);
    res = collect_task(info->t);

    if (res != PLANTOK) *(info->status_ptr) = 0; 
    else *(info->status_ptr) = 1; 

    return NULL;
}

int test_concurrent_clients() {
    printf("Test 8: Concurrent Clients ");
    fflush(stdout);

    pthread_t clients[10];
    worker_and_task_t args[10];
    int statuses[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    task_t tasks[10];
    worker_t workers[10];

    if (init_plant(stations, 10, 10) != PLANTOK) TEST_FAIL("Init failed");

    time_t now = time(NULL);

    pthread_barrier_t start_barrier;
    pthread_barrier_init(&start_barrier, NULL, 11);

    for(int i=0; i<10; i++) {
        tasks[i] = (task_t){.id = i, .start = now, .capacity = 1};
        workers[i] = (worker_t){.id = i + 10, .start = now, .end = now + 10, .work = work_sleep_1s};
        
        args[i] = (worker_and_task_t){
            .w = &workers[i], 
            .barier = &start_barrier,
            .status_ptr = &statuses[i], 
            .t = &tasks[i]
        };
        
        if (pthread_create(&clients[i], NULL, client_thread_func, &args[i]) != 0) {
            TEST_FAIL("Failed to create client thread");
        }
    }

    pthread_barrier_wait(&start_barrier);
    time_t start_time = time(NULL);
    
    for(int i=0; i<10; i++) {
        pthread_join(clients[i], NULL);
    }
    time_t end_time = time(NULL);

    destroy_plant();
    pthread_barrier_destroy(&start_barrier);

    double elapsed = difftime(end_time, start_time);

    for(int i=0; i<10; i++) {
        if (statuses[i] == 0) TEST_FAIL("Client failed to collect task");
    }

    for (int i = 0; i < 10; i++) {
        cleanup_task_memory(&tasks[i]);
    }

    if (elapsed < 3.0) {
        TEST_PASS();
        return 0;
    } else {
        printf("[Time: %.1fs] ", elapsed);
        TEST_FAIL("Too slow! Parallelism broken.");
    }
}
/// a czekamy na task kotry sie nigdy nie wydarzy 
void* stress_thread_func(void* arg) {
    worker_and_task_t* info = (worker_and_task_t*)arg;
    
    setup_task_memory(info->t, 1);
    
    pthread_barrier_wait(info->barier);

    int op = rand_r(&info->seed) % 5;
    switch(op) {
        case 0: 
        case 1: { 
            if (add_task(info->t) == PLANTOK) {
                collect_task(info->t);
            }
            break;
        }
        case 2: {
            init_plant(stations, n_stations, n_workers);
            break;
        }
        case 3: {
            destroy_plant();
            break;
        }
        case 4: { 
            add_worker(info->w);
            break;
        }
    }

    *(info->status_ptr) = 1; 
    return NULL;
}

int test_stress_mixed_ops(unsigned int main_seed) {
    printf("Test 10: Stress Mixed Ops (20 threads, seed: %u) ", main_seed);
    fflush(stdout);

    const int N = 20;
    pthread_t threads[N];
    worker_and_task_t args[N];
    int statuses[N];
    task_t tasks[N];
    worker_t workers[N];

    time_t now = time(NULL); 
    
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, N + 1);

    destroy_plant();

    for(int i = 0; i < N; i++) {
        tasks[i] = (task_t){.id = i, .start = now, .capacity = 1};
        workers[i] = (worker_t){.id = i + 100, .start = now, .end = now + 10, .work = work_sleep_1s};
        statuses[i] = 0;

        args[i] = (worker_and_task_t){
            .t = &tasks[i],
            .w = &workers[i],
            .barier = &barrier,
            .status_ptr = &statuses[i],
            // Tu jest klucz: seed zależy TYLKO od argumentu funkcji i indeksu pętli
            .seed = main_seed + i 
        };
        
        if (pthread_create(&threads[i], NULL, stress_thread_func, &args[i]) != 0) {
            TEST_FAIL("Failed to create thread");
        }
    }

    pthread_barrier_wait(&barrier);
    
    for(int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);
    destroy_plant();

    for(int i = 0; i < N; i++) {
        cleanup_task_memory(&tasks[i]);
    }

    TEST_PASS();
    return 0;
}



/* -------------------------------------------------------------------------- */
/*                                    Main                                    */
/* -------------------------------------------------------------------------- */

int main() {
    printf("==========================================\n");
    printf("     Advanced Plant Logic Tests           \n");
    printf("==========================================\n\n");

    int fail_count = 0;
    /*
    if (test_worker_ends_before_task_starts() != 0) fail_count++;
    if (test_destroy_waits_for_future() != 0) fail_count++;
    if (test_worker_starts_late() != 0) fail_count++;
    if (test_destroy_waits_multiple_future() != 0) fail_count++;
    if (test_worker_tight_schedule() != 0) fail_count++;
    if (test_mixed_destroy() != 0) fail_count++;
    if (test_massive_parallelism() != 0) fail_count++;
    */
    //if (test_concurrent_clients() != 0) fail_count++;
    printf("RANDOMIZED TESTS, IF THERE WAS ERROR ON TEST I, \n YOU CAN JUST CALL `test_stress_mixed_ops(i)` AND ANALYZE WITH VALGRIND\n ");
    printf("FOR EXAMPLE:  valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --fair-sched=yes --log-file=valgrind-out.txt ./demo");
    for (int i = 0; i < 100; i++) {
        if (test_stress_mixed_ops(i) != 0) fail_count++;
    }
    
   test_stress_mixed_ops(6);
    printf("\n==========================================\n");
    if (fail_count == 0) {
        printf(GREEN "ALL ADVANCED TESTS PASSED.\n" RESET);
        return 0;
    } else {
        printf(RED "%d TESTS FAILED.\n" RESET, fail_count);
        return 1;
    }
}