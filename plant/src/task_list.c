#include "../headers/task_list.h"
#include <stdlib.h>
#include "../headers/task_list.h"

int task_cont_init(task_container* cont)
{
    cont->capacity = 4;
    cont->count = 0;

    cont->items = malloc(cont->capacity * sizeof(task_info_t*));

    if (cont->items == NULL) 
        return -1;
    return 0;
}

int task_cont_push_back(task_container* cont, task_info_t* task)
{
    int id = task->original_def->id;
    for (size_t i = 0; i < cont->count; i++) {
        int cur_id = cont->items[i]->original_def->id;
        if (id == cur_id) {
            task_info_destroy(task);
            free(task);
            return 0;
        }
    }

    if (cont->count >= cont->capacity) {
        int new_capacity = cont->capacity * 2;
        task_info_t** new_items = realloc(cont->items, new_capacity * sizeof(task_info_t*));
        
        if (new_items == NULL) {
            return -1;
        }
        
        cont->items = new_items;
        cont->capacity = new_capacity;
    }

    cont->items[cont->count] = task;
    cont->count++;
    return 0;
}

task_info_t* task_cont_get(task_container* cont, size_t index)
{
    if (index >= cont->count) return NULL;
    return cont->items[index];
}

size_t task_cont_size(task_container* cont)
{
    return cont->count;
}

void task_cont_destroy(task_container* cont)
{

    for (size_t i = 0; i < cont->count; i++) {
        task_info_destroy(cont->items[i]); 
        free(cont->items[i]);
    }

    free(cont->items);
    cont->items = NULL;
    cont->count = 0;
    cont->capacity = 0;
}