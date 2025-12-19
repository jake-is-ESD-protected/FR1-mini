#ifndef _UII_H_
#define _UII_H_

#include "syserr.h"
#include "driver/gpio.h"
#include <inttypes.h>

#define UII_BIG_BUTTON_PIN (gpio_num_t)19
#define UII_SMALL_BUTTON_PIN (gpio_num_t)18
#define UII_EXTI_BIG_BUTTON_DEBOUNCE_TICKS      500
#define UII_EXTI_SMALL_BUTTON_DEBOUNCE_TICKS    500

typedef union {
    struct {
        uint16_t pin;
        uint16_t edge;
    } context;
    void* raw;
} uii_exti_context_t;

/// @brief 
/// @param  
/// @return 
e_syserr_t uii_exti_init(void);

#endif // _UII_H_