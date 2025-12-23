#include "../headers/worker_list.h"
#include <stdlib.h>

int worker_cont_init(worker_container* list, size_t n_workers)
{
    list->capacity = n_workers;
    list->count = 0;
    list->finished_workers = 0;

    list->items = malloc(list->capacity * sizeof(worker_info_t*));
    if (list->items == NULL) {
        list->capacity = 0;
        return -1;
    }
    return 0;
}

void worker_cont_push_back(worker_container* list, worker_info_t* worker)
{
    int id = worker->original_def->id;
    for(size_t i = 0; i < list->count; i++) {
        int curid = list->items[i]->original_def->id;
        if (id == curid) {
            worker_info_destroy(worker);
            free(worker);
            return;
        }
    }

    list->items[list->count] = worker;
    list->count++;
}

size_t worker_cont_size(worker_container* cont)
{
    return cont->count;
}

worker_info_t* worker_cont_get(worker_container* list, size_t index)
{
    if (index >= list->count) return NULL;
    return list->items[index];
}

void worker_cont_free(worker_container* list)
{
    for (size_t i = 0; i < list->count; i++) {
        worker_info_destroy(list->items[i]);
        free(list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->finished_workers = 0;
}