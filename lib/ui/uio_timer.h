#ifndef _UIO_TIMER_H_
#define _UIO_TIMER_H_

#include "driver/timer.h"
#include "syserr.h"

#define UIO_TIMER_GROUP      TIMER_GROUP_0
#define UIO_TIMER_NUM        TIMER_0
#define UIO_TIMER_FPS        (uint32_t)20
#define UIO_TIMER_US         (uint32_t)(1.0 / (float)UIO_TIMER_FPS * 1000 * 1000)
#define UIO_TIMER_UPDATE_SLOW_FAST_RATIO 100

typedef enum{
    uio_update_fast = 1,
    uio_update_mid = 5,
    uio_update_all = 100 // all includes slow updates
}uio_update_priority_t;

/// @brief Initialize the hardware timer for UI updates.
/// @return FR2 error code.
e_syserr_t uio_timer_init();

#endif // _UIO_TIMER_H_