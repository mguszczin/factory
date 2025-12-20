#ifndef FACTORY_INTERNAL_H
#define FACTORY_INTERNAL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "plant.h"
#include "../task_list_/task_list.h"
#include "../worker_list_/worker_list.h"


struct factory {
    pthread_mutex_t main_lock;
    bool is_terminated;

    const size_t* station_capacity;
    int* station_usage;
    size_t free_stations;
    size_t n_stations;

    task_container tasks;
    worker_container workers;

    pthread_cond_t manager_cond;
    pthread_t manager_thread;
};

#endif