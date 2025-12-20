#include "worker_list.h"

void worker_list_init(worker_container* list) {
    list->capacity = 4;
    list->count = 0;
    list->items = malloc(list->capacity * sizeof(worker_t*));
}

int worker_list_push_back(worker_container* list, worker_t* worker) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        
        worker_t** new_items = realloc(list->items, new_capacity * sizeof(worker_t*));
        
        if (new_items == NULL) {
            return -1;
        }
        
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = worker;
    list->count++;
    return 0;
}

worker_t* worker_list_get(worker_container* list, size_t index) {
    if (index >= list->count) return NULL;
    return list->items[index];
}

void worker_list_free(worker_container* list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}