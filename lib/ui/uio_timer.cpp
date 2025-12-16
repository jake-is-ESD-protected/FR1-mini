#include "uio.h"
#include "uio_timer.h"
#include "jescore.h"

static void IRAM_ATTR ui_timer_isr(void *p){
    static uint32_t idx = 1;
    uio_update_priority_t update_prio = uio_update_fast;
    timer_group_clr_intr_status_in_isr(UIO_TIMER_GROUP, UIO_TIMER_NUM);

    if (idx % uio_update_mid == 0) {
        update_prio = uio_update_mid;
    }
    
    if (idx % uio_update_all == 0) {
        update_prio = uio_update_all;
        idx = 0;
    }

    idx++;
    jes_notify_job_ISR(UIO_JOB_NAME, (uio_update_priority_t*)update_prio);    
    timer_group_enable_alarm_in_isr(UIO_TIMER_GROUP, UIO_TIMER_NUM);
}

e_syserr_t uio_timer_init(){
    timer_config_t cfg = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80, // 80 MHz APB clock / 80 = 1 MHz (1 tick = 1µs)
    #if SOC_TIMER_GROUP_SUPPORT_XTAL
        .clk_src = TIMER_SRC_CLK_APB,
    #endif
    };
    e_syserr_t e = e_syserr_driver_fail; 
    if(timer_init(UIO_TIMER_GROUP, UIO_TIMER_NUM, &cfg) != ESP_OK) { return e; }
    if(timer_set_counter_value(UIO_TIMER_GROUP, UIO_TIMER_NUM, 0) != ESP_OK) { return e; }
    if(timer_set_alarm_value(UIO_TIMER_GROUP, UIO_TIMER_NUM, UIO_TIMER_US) != ESP_OK) { return e; }
    if(timer_enable_intr(UIO_TIMER_GROUP, UIO_TIMER_NUM) != ESP_OK) { return e; }
    if(timer_isr_register(UIO_TIMER_GROUP, UIO_TIMER_NUM, ui_timer_isr, NULL, ESP_INTR_FLAG_IRAM, NULL) != ESP_OK) { return e; }
    if(timer_start(UIO_TIMER_GROUP, UIO_TIMER_NUM) != ESP_OK) { return e; }

    return e_syserr_none;
}