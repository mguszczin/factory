#include "../headers/factory.h"
#include "../../common/err.h"

#include <stdlib.h>
#include <string.h>

int factory_init(factory_t* f, int n_stations, int* station_capacities, int n_workers)
{
    f->n_stations = n_stations;
    f->is_active = true;
    f->is_terminated = false;

    f->station_capacity = malloc(sizeof(int) * n_stations);
    if (!f->station_capacity) {
        return -1;
    }
    memcpy(f->station_capacity, station_capacities, sizeof(int) * n_stations);

    f->station_usage = calloc(n_stations, sizeof(int));
    if (!f->station_usage) {
        free(f->station_capacity);
        return -1;   
    }

    if (task_cont_init(&f->tasks) != 0) {
        free(f->station_usage);
        free(f->station_capacity);
        return -1;
    }

    if (worker_cont_init(&f->workers, n_workers) != 0) {
        free(f->station_usage);
        free(f->station_capacity);
        task_cont_destroy(&f->tasks);
        return -1;
    }

    return 0;
}

/* Can't destory factory if it wasn't initialized before with condition */
void factory_destroy(factory_t* f)
{
    free(f->station_usage);
    free(f->station_capacity);
    f->station_capacity = NULL;
    f->station_usage = NULL;
    f->n_stations = 0;
    f->is_active = false;
    f->is_terminated = false;

    task_cont_destroy(&f->tasks);
    worker_cont_free(&f->workers);
}