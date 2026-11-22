#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    void systick_handler(void);
    uint64_t systick(void);

#ifdef __cplusplus
}
#endif
