#include "../headers/worker_info.h"
#include "../../common/err.h"

void worker_info_init(worker_info_t* info, worker_t* worker_def, struct factory_t* factory_t)
{
    info->original_def = worker_def;
    info->my_factory = factory_t;
    info->assigned_task = NULL;

    ASSERT_ZERO(pthread_mutex_init(&info->worker_mutex, NULL));
    ASSERT_ZERO(pthread_cond_init(&info->wakeup_cond, NULL));
}

void worker_info_destroy(worker_info_t* info)
{
    ASSERT_ZERO(pthread_mutex_destroy(&info->worker_mutex));
    ASSERT_ZERO(pthread_cond_destroy(&info->wakeup_cond));
}