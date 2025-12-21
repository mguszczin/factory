#ifndef FACTORY_H
#define FACTORY_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "task_list.h"
#include "worker_list.h"

typedef struct factory_struct {
    pthread_mutex_t main_lock;
    bool is_terminated;

    int* station_capacity;
    int* station_usage;
    size_t free_stations;
    size_t n_stations;

    task_container tasks;

    worker_container workers;

    pthread_cond_t manager_cond;
    pthread_t manager_thread;
} factory_t;

int factory_init(factory_t* f, size_t n_stations, int* station_capacities, size_t n_workers);
void factory_destroy(factory_t* f);

#endif