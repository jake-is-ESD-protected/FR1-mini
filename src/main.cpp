#include <Arduino.h>
#include <jescore.h>
#include "audio.h"
#include "fsm.h"
#include "fsm_jccl.h"
#include "syserr.h"
#include "wav.h"
#include "utils.h"
#include "sdcard.h"
#include "adc_base.h"
#include "uii.h"
#include "uio.h"
#include "uio_timer.h"

typedef e_syserr_t (*init_func)(void);

typedef enum e_fr1_module_t{
    e_fr1_module_audio,
    e_fr1_module_fsm,
    e_fr1_module_sdcard,
    e_fr1_module_adc,
    e_fr1_module_uii,
    e_fr1_module_uio,
    e_FR1_NUM_MODULES
}e_fr1_module_t;

static init_func init_funcs[e_FR1_NUM_MODULES] = {
    audio_init_default,
    fsm_init_default,
    sd_init_default,
    adc_base_init_default,
    uii_exti_init,
    uio_init
};

const char init_func_ids [e_FR1_NUM_MODULES][12] = {
    AUDIO_SERVER_JOB_NAME,
    FSM_CTRL_JOB_NAME,
    SDCARD_SERVER_JOB_NAME,
    ADC_BASE_JOB_NAME,
    "uii",
    "uio"
};

void fr1_system_init(void){
    e_syserr_t e;
    jes_err_t je;

    for(uint8_t i = 0; i < e_FR1_NUM_MODULES; i++){
        SCOPE_LOG_INIT(FR1_DEBUG_MSG_INFO "Initializing module <%s>", init_func_ids[i]);
        e = init_funcs[i]();
        if(e != e_syserr_none) { 
            SCOPE_LOG_INIT(FR1_DEBUG_MSG_FATAL "<%s> init fail (%d).", init_func_ids[i], e); 
            return; 
        }
    }
    uio_oled_title_screen();
    SCOPE_LOG_INIT(FR1_DEBUG_MSG_INFO "Starting audio engine...");
    jes_delay_job_ms(500);
    je = jes_launch_job(AUDIO_SERVER_JOB_NAME);
    if(je != e_err_no_err) { SCOPE_LOG_INIT(FR1_DEBUG_MSG_FATAL "Audio engine fail."); return; }

    for(uint8_t i = 0; i < FSM_JOB_N; i++){
        SCOPE_LOG_INIT(FR1_DEBUG_MSG_INFO "Launching state handler <%s>", fsm_jccl_jobs[i]);
        je = jes_launch_job(fsm_jccl_jobs[i]);
        if(je != e_err_no_err){ SCOPE_LOG_INIT("<%s> launch fail.", fsm_jccl_jobs[i]); return; }
        jes_delay_job_ms(400);
    }
    uio_oled_idle_screen();

    SCOPE_LOG_INIT(FR1_DEBUG_MSG_INFO "Dispatching UI update timer...");
    je = jes_launch_job(UIO_JOB_NAME);
    if(je != e_err_no_err) { 
        SCOPE_LOG_INIT(FR1_DEBUG_MSG_FATAL "UI launch fail (%d).", e); 
        return; 
    }
    e = uio_timer_init();
    if(e != e_syserr_none) { 
        SCOPE_LOG_INIT(FR1_DEBUG_MSG_FATAL "UI timer init fail (%d).", e); 
        return; 
    }
    SCOPE_LOG_INIT(FR1_DEBUG_MSG_INFO "|O === FR1 === O|");
}

void setup(){
    jes_delay_job_ms(500);
    jes_err_t e = jes_init();
    if(e != e_err_no_err){
        while(1);
    }
    fr1_system_init();
}

void loop(){

}