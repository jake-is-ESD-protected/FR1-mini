#include <jescore.h>
#include <Arduino.h>
#include "adc_base.h"
#include "syserr.h"
#include "fsm.h"

static uint8_t __init = 0;

e_syserr_t adc_base_init(uint8_t pin){
    if(pin != ADC_LIPO_LEVEL_PIN && pin != ADC_PLUG_DETECT_PIN){
        return e_syserr_param;
    }
    pinMode(pin, INPUT);
    jes_err_t je = jes_register_job(ADC_BASE_JOB_NAME, 2048, 1, adc_base_job, 0);
    // allow multiple calls (multiple ADCs)
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    __init = 1;
    return e_syserr_none;
}

e_syserr_t adc_base_init_default(void){
    e_syserr_t e = adc_base_init(ADC_LIPO_LEVEL_PIN);
    if(e != e_syserr_none) return e;
    e = adc_base_init(ADC_PLUG_DETECT_PIN);
    if(e != e_syserr_none) return e;
    return e_syserr_none;
}

uint32_t adc_base_get_mv(uint8_t pin){
    if(!__init) return 0;
    // All ADCs are connected to 50/50 voltage dividers
    return analogReadMilliVolts(pin) * 2;
}

void adc_base_job(void* p){
    job_struct_t* pj = (job_struct_t*)p;
    if(!__init){
        SCOPE_LOG_PJ(pj, "ADC not initialized!");
        jes_throw_error((jes_err_t)e_syserr_uninitialized);
        return;
    }
    char* args = jes_job_get_args();
    char* arg = strtok(args, " ");
    e_syserr_t e;
    uint32_t pin;
    if(!arg) arg = (char*)" ";
    if(strcmp(arg, "lipo") == 0){
        pin = ADC_LIPO_LEVEL_PIN;

    }
    else if(strcmp(arg, "plug") == 0){
        pin = ADC_PLUG_DETECT_PIN;
    }
    else{
        SCOPE_LOG_PJ(pj, "Unknown probe <%s>, use <lipo> or <plug>", arg);
        jes_throw_error((jes_err_t)e_syserr_param);
        return;
    }

    SCOPE_LOG_PJ(pj, "Level at probe: %d mV", adc_base_get_mv(pin));
}