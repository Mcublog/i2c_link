#pragma once

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void systime_init(void);
    bool systime_is_need_to_init(void);
    void systime_sync(time_t t);

#ifdef __cplusplus
}
#endif
