#ifndef WORKER_INFO_H
#define WORKER_INFO_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "../../common/plant.h"
#include "task_info.h"

typedef struct {
    worker_t* original_def;
    pthread_t thread_id;
    
    pthread_cond_t wakeup_cond;
    
    int assigned_index;
    task_info_t* assigned_task;
} worker_info_t;

int worker_info_init(worker_info_t* info, worker_t* worker_def);
void worker_info_destroy(worker_info_t* info);

#endif