#ifndef FACTORY_H
#define FACTORY_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "task_list.h"
#include "worker_list.h"

typedef struct {
    pthread_mutex_t main_lock;
    _Atomic bool is_terminated;

    int* station_capacity;
    int* station_usage;
    size_t free_stations;
    size_t n_stations;

    task_container tasks;
    worker_container workers;

    pthread_cond_t manager_cond;
} factory_t;

void factory_init(struct factory_t* f, size_t n_stations, const size_t* station_capacities);
void factory_destroy(struct factory_t* f);

#endif