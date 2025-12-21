#include <jescore.h>
#include "uio.h"
#include "uio_timer.h"
#include "fsm.h"
#include "bitmaps.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Wire.h"
#include "adc_base.h"
#include "dsp_fr1.h"

Adafruit_SSD1306 oled(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, &Wire, OLED_RESET);

e_syserr_t uio_init(void){
    jes_err_t je = jes_register_job(UIO_JOB_NAME, 2048, 1, uio_job, 1);
    if(je != e_err_no_err) return (e_syserr_t)je;
    pinMode(UIO_LED_PIN, OUTPUT);
    uio_oled_init();
    return e_syserr_none;
}

void uio_oled_init(void){
    Wire.begin();
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)){
        // ret err
    }
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    // FR1 mini screen is soldered upside down
    oled.setRotation(2);
    // Set support for 64x48 on HW level
    oled.ssd1306_command(SSD1306_SETMULTIPLEX);
    oled.ssd1306_command(0x2F);  // 48-1 = 47
    oled.ssd1306_command(SSD1306_SETDISPLAYOFFSET);
    oled.ssd1306_command(0x00);  // No offset
    oled.ssd1306_command(SSD1306_SETCOMPINS);
    oled.ssd1306_command(0x12);
    oled.display();
}

void uio_led_on(void){
    digitalWrite(UIO_LED_PIN, HIGH);
}

void uio_led_off(void){
    digitalWrite(UIO_LED_PIN, LOW);
}

void uio_led_toggle(void){
    static uint8_t on = 0;
    on = !on;
    digitalWrite(UIO_LED_PIN, on);
}

void uio_led_level(uint8_t lvl){
    analogWrite(UIO_LED_PIN, lvl);
}

void uio_oled_draw_widgets_all(void){
    oled.drawBitmap(BATTERY_POS_X, BATTERY_POS_Y, battery, 
        BATTERY_WIDTH, BATTERY_HEIGHT, WHITE);
    oled.drawBitmap(SETTINGS_POS_X, SETTINGS_POS_Y, settings, 
        SETTINGS_WIDTH, SETTINGS_HEIGHT, WHITE);
    oled.drawBitmap(FILES_POS_X, FILES_POS_Y, files, 
        FILES_WIDTH, FILES_HEIGHT, WHITE);
}

void uio_oled_title_screen(void){
    oled.clearDisplay();
    oled.display();
    oled.drawBitmap(0, 0, title_screen, 
        SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);
    oled.display();
}

void uio_oled_idle_screen(void){
    oled.clearDisplay();
    oled.drawBitmap(0, 0, idle_screen, 
        SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);
    uio_oled_draw_widgets_all();
}

void uio_oled_rec_screen(void){
    oled.clearDisplay();
    oled.drawBitmap(0, 0, rec_screen, 
        SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, WHITE);
    uio_oled_draw_widgets_all();
}

void uio_oled_sett_screen(void){
    oled.clearDisplay();
    // draw bitmap here
}

void uio_oled_update_db(int16_t val){
    static uint8_t soft_clk_div = 0;
    if(soft_clk_div == OLED_VISUAL_TXT_UPD){
        uio_oled_update_db_text(val);
        soft_clk_div = 0;
    }
    uio_oled_update_db_vu(val);
    soft_clk_div++;
}

void uio_oled_update_db_text(int16_t val){
    oled.fillRect(0, 0, 40, 8, BLACK);
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0,0);
    oled.printf("%d dB(Z)", val);
}

void uio_oled_update_battery(uint16_t val){
    if(val > ADC_LIPO_LVL_MAX_MV) val = ADC_LIPO_LVL_MAX_MV;
    int16_t range = map(val, 
                         ADC_LIPO_LVL_MIN_MV, 
                         ADC_LIPO_LVL_MAX_MV, 
                         0,
                         UIO_OLED_WGT_BATT_W);
    oled.fillRect(UIO_OLED_WGT_BATT_X,
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
    oled.drawLine(OLED_VISUAL_VUM_X, OLED_VISUAL_VUM_Y, tip_x_prev, tip_y_prev, BLACK);
    oled.drawLine(OLED_VISUAL_VUM_X, OLED_VISUAL_VUM_Y, tip_x, tip_y, WHITE);
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

        // set up page
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

        // idle routine
        if(rta.cur_state == e_fsm_state_idle){
            uio_oled_update_db_vu((int16_t)DSP_FR1_DBFS_TO_SPL(rtv.dbfs_avg.l));
            if(prio == uio_update_mid){
                uio_oled_update_db_text((int16_t)DSP_FR1_DBFS_TO_SPL(rtv.dbfs_avg.l));
            }
            if(prio == uio_update_all){
                
            }
        }

        // rec routine
        if(rta.cur_state == e_fsm_state_rec){
            oled.setTextSize(1);
            oled.fillRect(0, 10, 53, 8, BLACK);
            uint32_t total_seconds = rtv.t_transaction / 1000;
            uint8_t hours = total_seconds / 3600;
            uint8_t minutes = (total_seconds % 3600) / 60;
            uint8_t seconds = total_seconds % 60;
            uint8_t ms = (rtv.t_transaction % 1000) / 10;
            // oled.fillRect(0, 26, 23, 8, BLACK);
            oled.setCursor(0, 10);
            oled.printf("%02d:%02d:%02d", minutes, seconds, ms);
            // oled.printf("sys: \n\r%d s\n\r", rtv.t_system / 1000);
            if(prio == uio_update_mid){

            }
            if(prio == uio_update_all){
                
            }
        }

        // sett routine
        if(rta.cur_state == e_fsm_state_sett){

            if(prio == uio_update_mid){
                oled.drawBitmap(0, 0, fr1_buddy, 
                    FR1_BUDDY_WIDTH, FR1_BUDDY_HEIGHT, WHITE);
                // oled.fillRect(0, 10, 23, 8, BLACK);
                // oled.fillRect(0, 26, 23, 8, BLACK);
                // oled.setCursor(1, 2);
                // oled.printf("LiPo: \n\r%d mV\n\r", rtv.lipo_mv);
                // oled.printf("Plug: \n\r%d mV\n\r", rtv.plug_mv);
            }
            if(prio == uio_update_all){
                
            }
        }


        if(prio == uio_update_all){
            uio_oled_update_battery(rtv.lipo_mv);
            if(rtv.lipo_mv < 3700){
                // low battery popup
            }
        }
        oled.display();
        rta_old = rta;
    }
}
