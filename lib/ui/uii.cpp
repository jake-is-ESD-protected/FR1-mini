#include <Arduino.h>
#include <jescore.h>
#include "uii.h"
#include "fsm.h"

TickType_t uii_small_button_ts = 0;
TickType_t uii_big_button_ts = 0;

/// @brief 
/// @param p 
/// @return 
static inline void IRAM_ATTR uii_exti_handler(void* p);

/// @brief 
/// @param ts 
/// @return 
static inline void IRAM_ATTR uii_big_big_button_handler(TickType_t ts);

static inline e_syserr_t ui_exti_init_pin(gpio_int_type_t edge, gpio_num_t pin, void(*handler)(void*),gpio_pullup_t pullup){
    gpio_config_t io_conf;
    io_conf.intr_type = edge;
    io_conf.pin_bit_mask = 1ULL << (uint32_t)pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = pullup;
    if(gpio_config(&io_conf) != ESP_OK) { return e_syserr_param; }
    uii_exti_context_t ctxt;
    ctxt.context.pin = (uint16_t)pin;
    ctxt.context.edge = (uint16_t)edge;
    if(gpio_isr_handler_add(pin, handler, (void*)ctxt.raw) != ESP_OK) { return e_syserr_driver_fail; }
    return e_syserr_none;
}


e_syserr_t uii_exti_init(void){
    e_syserr_t stat;
    if(gpio_install_isr_service(0) != ESP_OK) { return e_syserr_driver_fail; }
    if((stat = ui_exti_init_pin(GPIO_INTR_NEGEDGE, UII_BIG_BUTTON_PIN, uii_exti_handler, GPIO_PULLUP_ENABLE)) != e_syserr_none) { return stat; }
    if((stat = ui_exti_init_pin(GPIO_INTR_POSEDGE, UII_BIG_BUTTON_PIN, uii_exti_handler, GPIO_PULLUP_ENABLE)) != e_syserr_none) { return stat; }
    if((stat = ui_exti_init_pin(GPIO_INTR_NEGEDGE, UII_SMALL_BUTTON_PIN, uii_exti_handler, GPIO_PULLUP_ENABLE)) != e_syserr_none) { return stat; }
    if((stat = ui_exti_init_pin(GPIO_INTR_POSEDGE, UII_SMALL_BUTTON_PIN, uii_exti_handler, GPIO_PULLUP_ENABLE)) != e_syserr_none) { return stat; }
    return e_syserr_none;
}


static inline void IRAM_ATTR uii_exti_handler(void* p){
    static uii_exti_context_t old_ctxt;
    old_ctxt.raw = 0;
    static uii_exti_context_t ctxt;
    ctxt.raw = p;
    gpio_num_t pin = (gpio_num_t)ctxt.context.pin;
    gpio_int_type_t edge = (gpio_int_type_t)ctxt.context.edge;
    TickType_t now = xTaskGetTickCountFromISR();
    switch(pin){
        case UII_BIG_BUTTON_PIN:
            if(now - uii_small_button_ts > UII_EXTI_BIG_BUTTON_DEBOUNCE_TICKS){

                if(ctxt.context.edge == GPIO_INTR_NEGEDGE){

                }
                else{
                    jes_launch_job_args(FSM_CTRL_JOB_NAME, FSM_RECORDING_JOB_NAME " toggle");
                    // jes_launch_job_args(FSM_CTRL_JOB_NAME, FSM_SETTINGS_JOB_NAME);
                }
                uii_small_button_ts = now;
            }
            break;
        case UII_SMALL_BUTTON_PIN:
            if(now - uii_big_button_ts > UII_EXTI_SMALL_BUTTON_DEBOUNCE_TICKS){

                if(ctxt.context.edge == GPIO_INTR_NEGEDGE){

                }
                else{
                    jes_launch_job_args(FSM_CTRL_JOB_NAME, FSM_SETTINGS_JOB_NAME);
                }
                uii_big_button_ts = now;
            }
            
        break;
        default:
        break;
    }
    old_ctxt = ctxt;
}
