#include "../headers/worker_info.h"
#include "../../common/err.h"

int worker_info_init(worker_info_t* info, worker_t* worker_def, const struct factory_struct* f)
{
    info->original_def = worker_def;
    info->my_factory = f;
    info->assigned_task = NULL;

    if (pthread_cond_init(&info->wakeup_cond, NULL) != 0) {
        info->original_def = NULL;
        info->my_factory = NULL;
        info->assigned_task = NULL;
        return -1;
    }
    return 0;
}

void worker_info_destroy(worker_info_t* info)
{
    info->thread_id = NULL;
    info->original_def = NULL;
    info->my_factory = NULL;
    info->assigned_task = NULL;

    ASSERT_ZERO(pthread_cond_destroy(&info->wakeup_cond));
}