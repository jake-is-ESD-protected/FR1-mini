#include <jescore.h>
#include "uio.h"
#include "uio_timer.h"
#include "fsm.h"
#include "bitmaps.h"
#include "SparkFun_Qwiic_OLED.h"
#include "Wire.h"
#include "adc_base.h"
#include "dsp_fr1.h"

QwiicMicroOLED oled;

e_syserr_t uio_init(void){
    jes_err_t je = jes_register_job(UIO_JOB_NAME, 2048, 1, uio_job, 1);
    if(je != e_err_no_err) return (e_syserr_t)je;
    uio_oled_init();
    return e_syserr_none;
}

void uio_oled_init(void){
    Wire.begin();
    if (!oled.begin()){
        // ret err
    }
    oled.display();
}

void uio_oled_rotate_show(void){
    // oled.flipHorizontal(true);
    // oled.flipVertical(true);
    oled.display();
}

void uio_oled_title_screen(void){
    oled.reset(true);
    oled.bitmap(0, 0, BMP_TITLE_SCREEN);
    uio_oled_rotate_show();
}

void uio_oled_idle_screen(void){
    oled.reset(true);
    oled.bitmap(0, 0, BMP_IDLE_SCREEN);
    uio_oled_rotate_show();
}

void uio_oled_rec_screen(void){
    oled.reset(true);
    // draw bitmap here
    uio_oled_rotate_show();
}

void uio_oled_sett_screen(void){
    oled.reset(true);
    // draw bitmap here
    uio_oled_rotate_show();
}

void uio_oled_update_db(int16_t val){
    static uint8_t soft_clk_div = 0;
    if(soft_clk_div == OLED_VISUAL_TXT_UPD){
        uio_oled_update_db_text(val);
        soft_clk_div = 0;
    }
    uio_oled_update_db_vu(val);
    soft_clk_div++;
    uio_oled_rotate_show();
}

void uio_oled_update_db_text(int16_t val){
    oled.rectangleFill(0, 0, 40, 8, COLOR_BLACK);
    oled.setCursor(0,0);
    oled.printf("%d dB", val);
}

void uio_oled_update_battery(uint16_t val){
    if(val > ADC_LIPO_LVL_MAX_MV) val = ADC_LIPO_LVL_MAX_MV;
    int16_t range = map(val, 
                         ADC_LIPO_LVL_MIN_MV, 
                         ADC_LIPO_LVL_MAX_MV, 
                         0,
                         UIO_OLED_WGT_BATT_W);
    oled.rectangleFill(UIO_OLED_WGT_BATT_X,
                       UIO_OLED_WGT_BATT_Y, 
                       range, 
                       UIO_OLED_WGT_BATT_H, 
                       1);
}

void uio_oled_update_db_vu(int16_t val){
    float rad = uio_oled_db_to_deg(val) * (PI / 180.0);
    static uint8_t tip_x_prev = OLED_VISUAL_VUM_X;
    static uint8_t tip_y_prev = OLED_VISUAL_VUM_Y;
    uint8_t tip_x = 32 + (int8_t)(OLED_VISUAL_VUM_RAD * cosf(rad));
    uint8_t tip_y = 48 - (uint8_t)(OLED_VISUAL_VUM_RAD * sinf(rad));
    oled.line(OLED_VISUAL_VUM_X, OLED_VISUAL_VUM_Y, tip_x_prev, tip_y_prev, COLOR_BLACK);
    oled.line(OLED_VISUAL_VUM_X, OLED_VISUAL_VUM_Y, tip_x, tip_y, COLOR_WHITE);
    tip_x_prev = tip_x;
    tip_y_prev = tip_y;
}

uint8_t uio_oled_db_to_deg(int16_t db){
    float degreeRange = OLED_VISUAL_DEG_MAX - OLED_VISUAL_DEG_MIN;
    float decibelRange = OLED_VISUAL_DB_MAX - OLED_VISUAL_DB_MIN;
    float degrees = OLED_VISUAL_DEG_MAX - (((db - OLED_VISUAL_DB_MIN) / decibelRange) * degreeRange);
    return (uint8_t)degrees;
}

void uio_job(void* p){
    job_struct_t* pj = (job_struct_t*)p;
    pj->role = e_role_core;
    static uio_update_priority_t prio;
    static fsm_runtime_args_t rta_old;
    static fsm_state_t state_new;
    while(1){
        prio = (uio_update_priority_t)(uint32_t)jes_wait_for_notification();
        fsm_runtime_args_t rta = fsm_get_runtime_args();
        fsm_runtime_values_t rtv = fsm_get_runtime_values();

        if(rta.cur_state != rta_old.cur_state){
            // on change event
            switch (rta.cur_state)
            {
            case e_fsm_state_idle:
                uio_oled_idle_screen();
                break;
            
            case e_fsm_state_rec:
                uio_oled_rec_screen();
                break;
            
            case e_fsm_state_sett:
                uio_oled_sett_screen();
                break;
            default:
                break;
            }            
        }

        // idle
        if(rta.cur_state == e_fsm_state_idle){
            uio_oled_update_db_vu((int16_t)DSP_FR1_DBFS_TO_SPL(rtv.dbfs_avg.l));
            if(prio == uio_update_mid){
                uio_oled_update_db_text((int16_t)DSP_FR1_DBFS_TO_SPL(rtv.dbfs_avg.l));
            }
            if(prio == uio_update_all){
                
            }
        }

        // rec
        if(rta.cur_state == e_fsm_state_rec){

            if(prio == uio_update_mid){

            }
            if(prio == uio_update_all){
                
            }
        }

        // sett
        if(rta.cur_state == e_fsm_state_sett){

            if(prio == uio_update_mid){
                uint32_t lipo_mv = adc_base_get_mv(ADC_LIPO_LEVEL_PIN);
                uint32_t plug_mv = adc_base_get_mv(ADC_PLUG_DETECT_PIN);
                oled.rectangleFill(0, 10, 23, 8, COLOR_BLACK);
                oled.rectangleFill(0, 26, 23, 8, COLOR_BLACK);
                oled.setCursor(1, 2);
                oled.printf("LiPo: \n\r%d mV\n\r", lipo_mv);
                oled.printf("Plug: \n\r%d mV\n\r", plug_mv);
            }
            if(prio == uio_update_all){
                
            }
        }


        if(prio == uio_update_all){
            uint32_t lipo_mv = adc_base_get_mv(ADC_LIPO_LEVEL_PIN);
            adc_base_get_mv(ADC_PLUG_DETECT_PIN);
            uio_oled_update_battery(lipo_mv);
        }
        uio_oled_rotate_show();
        rta_old = rta;
    }
}

void uio_oled_refresh_test(void){
    unsigned long start_time, end_time, refresh_time;
    oled.reset(true);
    start_time = millis();
    for(int i=0; i<48; i++){
        oled.pixel(i, i);
        uio_oled_rotate_show();
    }
    end_time = millis();
    refresh_time = end_time - start_time;
    oled.reset(true);
    oled.setCursor(0,0);
    oled.print("Refresh \n\rtime:\n\r");
    oled.print(refresh_time);
    oled.print(" ms");
    uio_oled_rotate_show();
}