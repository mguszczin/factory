#ifndef WORKER_LIST_H
#define WORKER_LIST_H

#include <stdlib.h>
#include "plant.h"

typedef struct {
    worker_t** items;
    size_t capacity;
    size_t count;
    size_t finished_workers;
} worker_container;

void worker_list_init(worker_container* list);
int worker_list_push_back(worker_container* list, worker_t* worker);
worker_t* worker_list_get(worker_container* list, size_t index);
void worker_list_free(worker_container* list);

#endif