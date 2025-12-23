#include "../headers/worker_info.h"
#include "../../common/err.h"

int worker_info_init(worker_info_t* info, worker_t* worker_def)
{
    info->original_def = worker_def;
    info->assigned_task = NULL;

    if (pthread_cond_init(&info->wakeup_cond, NULL) != 0) {
        info->original_def = NULL;
        info->assigned_task = NULL;
        return -1;
    }
    return 0;
}

void worker_info_destroy(worker_info_t* info)
{
    info->original_def = NULL;
    info->assigned_task = NULL;

    ASSERT_ZERO(pthread_cond_destroy(&info->wakeup_cond));
}