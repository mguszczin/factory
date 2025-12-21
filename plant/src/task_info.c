#include "../headers/task_info.h"
#include "../../common/err.h"

void task_info_init(task_info_t* info, task_t* task_def)
{
    info->original_def = task_def;
    info->workers_assigned = 0;
    info->workers_finished = 0;
    info->assigned_position = 0;
    info->is_completed = false;

    ASSERT_ZERO(pthread_mutex_init(&info->task_mutex, NULL));
    ASSERT_ZERO(pthread_cond_init(&info->task_complete_cond, NULL));
}

void task_info_destroy(task_info_t* info)
{
    ASSERT_ZERO(pthread_mutex_destroy(&info->task_mutex));
    ASSERT_ZERO(pthread_cond_destroy(&info->task_complete_cond));
}