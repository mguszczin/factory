#include "../headers/task_list.h"
#include <stdlib.h>

void taks_cont_init(task_container* list) {
    list->capacity = 4;
    list->count = 0;
    list->items = malloc(list->capacity * sizeof(task_info_t*));
}

int task_cont_push_back(task_container* list, task_info_t* task) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        task_info_t** new_items = realloc(list->items, new_capacity * sizeof(task_info_t*));
        
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

task_info_t* task_cont_get(task_container* list, size_t index) {
    if (index >= list->count) return NULL;
    return list->items[index];
}

void task_cont_free(task_container* list) {
    if (list->items != NULL) {
        free(list->items);
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}