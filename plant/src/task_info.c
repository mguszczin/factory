#include "../headers/task_info.h"
#include "../../common/err.h"

int task_info_init(task_info_t* info, task_t* task_def)
{
    info->original_def = task_def;
    info->workers_assigned = 0;
    info->is_completed = false;
    info->failed = false;

    if (pthread_cond_init(&info->task_complete_cond, NULL) != 0) {
        info->original_def = NULL;
        return -1;
    }

    return 0;
}

void task_info_destroy(task_info_t* info)
{
    info->original_def = NULL;
    info->workers_assigned = 0;
    /* Manager will ignore it */
    info->is_completed = true;
    info->failed = false;
    ASSERT_ZERO(pthread_cond_destroy(&info->task_complete_cond));
}