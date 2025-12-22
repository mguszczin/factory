#ifndef FACTORY_H
#define FACTORY_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "task_list.h"
#include "worker_list.h"

typedef struct factory_struct {
    /* Tere is a case where factory might be 
       terminated but still active */
    bool is_active;
    bool is_terminated;

    int* station_capacity;
    int* station_usage;
    int n_stations;

    task_container tasks;

    worker_container workers;

    pthread_cond_t manager_cond;
    pthread_t manager_thread;
} factory_t;

/* No condition initialized here. we will do this inside mutex */
int factory_init(factory_t* f, int n_stations, int* station_capacities, int n_workers);
void factory_destroy(factory_t* f);

#endif