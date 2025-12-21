#include "../headers/factory.h"
#include "../../common/err.h"
#include <string.h>

#include "../headers/factory.h"
#include "../../common/err.h"
#include <string.h>

int factory_init(factory_t* f, size_t n_stations, int* station_capacities, size_t n_workers)
{
    f->n_stations = n_stations;
    f->free_stations = n_stations;
    
    f->is_terminated = false;

    f->station_capacity = malloc(n_stations * sizeof(int));
    if (!f->station_capacity) 
        return -1;
    
    memcpy(f->station_capacity, station_capacities, n_stations * sizeof(int));

    f->station_usage = calloc(n_stations, sizeof(int));
    if (!f->station_usage) {
        free(f->station_capacity);
        return -1;   
    }

    ASSERT_ZERO(pthread_mutex_init(&f->main_lock, NULL));
    ASSERT_ZERO(pthread_cond_init(&f->manager_cond, NULL));

    ASSERT_ZERO(task_cont_init(&f->tasks));
    ASSERT_ZERO(worker_cont_init(&f->workers, n_workers));

    return 0;
}

void factory_destroy(factory_t* f)
{
    free(f->station_capacity);
    free(f->station_usage);
    f->station_capacity = NULL;
    f->station_usage = NULL;

    ASSERT_ZERO(pthread_mutex_destroy(&f->main_lock));
    ASSERT_ZERO(pthread_cond_destroy(&f->manager_cond));

    task_cont_free(&f->tasks);
    worker_cont_free(&f->workers);
}