#include "task_list.h"
#include <stdlib.h>

void task_list_init(task_container* list) {
    list->capacity = 4;
    list->count = 0;
    list->items = malloc(list->capacity * sizeof(task_t*));
}

int task_list_push_back(task_container* list, task_t* task) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        task_t** new_items = realloc(list->items, new_capacity * sizeof(task_t*));
        
        if (new_items == NULL) {
            return -1;
        }
        
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = task;
    list->count++;
    return 0;
}

task_t* task_list_get(task_container* list, size_t index) {
    if (index >= list->count) return NULL;
    return list->items[index];
}

void task_list_free(task_container* list) {
    if (list->items != NULL) {
        free(list->items);
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}