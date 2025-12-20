#include "../headers/factory.h"
#include "../../common/err.h"
#include <string.h>

void factory_init(struct factory_t* f, size_t n_stations, size_t n_workers, const size_t* station_capacities) {
    ASSERT_ZERO(pthread_mutex_init(&f->main_lock, NULL));
    ASSERT_ZERO(pthread_cond_init(&f->manager_cond, NULL));

    f->is_terminated = false;
    f->n_stations = n_stations;
    f->free_stations = n_stations;

    const size_t* station_capacities = 

    

    ASSERT_SYS_OK(taks_cont_init(&f->tasks));
    ASSERT_SYS_OK(worker_cont_init(&f->workers));
}

void factory_destroy(struct factory_t* f) {
    task_cont_free(&f->tasks);
    worker_cont_free(&f->workers);

    free(f->station_usage);
    f->station_usage = NULL;

    free((void*)f->station_capacity);
    f->station_capacity = NULL;

    ASSERT_ZERO(pthread_mutex_destroy(&f->main_lock));
    ASSERT_ZERO(pthread_cond_destroy(&f->manager_cond));
}