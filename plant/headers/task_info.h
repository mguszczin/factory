#ifndef TASK_INFO_H
#define TASK_INFO_H

#include <pthread.h>
#include <stdbool.h>
#include "../../common/plant.h"

typedef struct {
    task_t* original_def;
    size_t assigned_position;
    
    pthread_mutex_t task_mutex; 
    pthread_cond_t task_complete_cond;
    int workers_assigned;
    bool is_completed;
    bool failed;
} task_info_t;

void task_info_init(task_info_t* info, task_t* task_def);
void task_info_destroy(task_info_t* info);

#endif