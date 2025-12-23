#ifndef WORKER_LIST_H
#define WORKER_LIST_H

#include "worker_info.h"

typedef struct {
    worker_info_t** items;
    size_t capacity;
    size_t count;

    size_t finished_workers;
} worker_container;

int worker_cont_init(worker_container* cont, size_t n_workers);
void worker_cont_push_back(worker_container* cont, worker_info_t* worker);
size_t worker_cont_size(worker_container* cont);
worker_info_t* worker_cont_get(worker_container* cont, size_t index);
void worker_cont_free(worker_container* cont);

#endif