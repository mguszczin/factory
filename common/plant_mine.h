#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "plant.h"

struct factory {
    pthread_mutex_t main_lock;
    bool is_terminated;

    const size_t* station_capacity;
    int* station_usage;
    size_t free_stations, n_statation;

    task_containter tasks;
    worker_container workers;

    pthread_cond_t manager_cond;
    pthread_t manager_thread;
};