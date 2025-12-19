#include <Arduino.h>
#include <jescore.h>
#include "audio.h"
#include <soc/i2s_reg.h>
#include "fsm.h"

QueueHandle_t audio_evt_queue_in;

e_syserr_t audio_init(uint32_t sampleRate, uint8_t bclk, uint8_t ws, uint8_t data_rx){

    if(!AUDIO_SR_VALID(sampleRate)) return e_syserr_param;
    esp_err_t e;
    jes_err_t je = jes_register_job(AUDIO_SERVER_JOB_NAME, AUDIO_SERVER_JOB_MEM, 1, audio_sampler, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;

    if(__job_get_job_by_name(AUDIO_SERVER_JOB_NAME)->instances){
        /*If the audio loop is already running, send a restart signal that delays
        the next execution of the loop until the new driver settings are applied.*/
        i2s_event_t evt;
        evt.type = (i2s_event_type_t)I2S_EVENT_RESTART;
        xQueueSend(audio_evt_queue_in, &evt, portMAX_DELAY);
        jes_delay_job_ms(AUDIO_I2S_RESTART_MS / 5);
        e = i2s_driver_uninstall(AUDIO_I2S_PORT);
        if(e != ESP_OK) return e_syserr_driver_fail;
        audio_evt_queue_in = NULL;
    }

    const i2s_config_t audio_cfg = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    i2s_pin_config_t audio_pins = {
        .bck_io_num = bclk,
        .ws_io_num = ws,
        .data_out_num = -1,
        .data_in_num = data_rx,
    };

    e = i2s_driver_install(AUDIO_I2S_PORT, &audio_cfg, 4, &audio_evt_queue_in);
    if (e != ESP_OK) {
        return e_syserr_driver_fail;
    }
    
    // // Delay by falling edge
    // REG_SET_BIT(I2S_RX_TIMING_REG(AUDIO_I2S_PORT), BIT(1));
    // // Force Philips mode
    // REG_SET_BIT(I2S_RX_CONF1_REG(AUDIO_I2S_PORT), I2S_RX_MSB_SHIFT);

    e = i2s_set_pin(AUDIO_I2S_PORT, &audio_pins);
    if (e != ESP_OK) { 
        return e_syserr_driver_fail;
    }
    return e_syserr_none;
}

e_syserr_t audio_init_default(void){
    return audio_init(AUDIO_SR_DEFAULT, 
                      AUDIO_PIN_MEMS_I2S_BCLK, 
                      AUDIO_PIN_MEMS_I2S_WS, 
                      AUDIO_PIN_MEMS_I2S_IN);
}

void audio_sampler(void* p){
    static uint8_t act = 0;
    act = !act;
    job_struct_t* pj = (job_struct_t*)p;
    pj->role = e_role_core;
    while(act){
        i2s_event_t evt;
        fsm_state_struct_t* state = (fsm_state_struct_t*) jes_job_get_param();
        static bool tx_occ = false;
        static bool rx_occ = false;
        if (xQueueReceive(audio_evt_queue_in, &evt, portMAX_DELAY) == pdPASS){
            if(evt.type == (i2s_event_type_t)I2S_EVENT_RESTART){
                jes_delay_job_ms(AUDIO_I2S_RESTART_MS);
                SCOPE_LOG_PJ(pj, "Audio was restarted!");
                continue;
            }
            state->routine(&state->rt_args);
            // if (evt.type != I2S_EVENT_TX_DONE && evt.type != I2S_EVENT_RX_DONE){
            //     continue;
            // }
            // if(evt.type == I2S_EVENT_TX_DONE) tx_occ = true;
            // if(evt.type == I2S_EVENT_RX_DONE) rx_occ = true;

            // if(tx_occ && rx_occ){
            //     state->routine(&state->rt_args);
            //     tx_occ = false;
            //     rx_occ = false;
            // } 
        }
    }
}

void audio_read(stereo_sample_t *data, uint32_t len){
    uint32_t bytesRead = 0;
    esp_err_t e = i2s_read(AUDIO_I2S_PORT, (uint8_t *)data, len * sizeof(stereo_sample_t), &bytesRead, portMAX_DELAY);
}

void audio_suspend_short(void){
    i2s_event_t evt;
    evt.type = (i2s_event_type_t)I2S_EVENT_RESTART;
    xQueueSend(audio_evt_queue_in, &evt, portMAX_DELAY);
}

void audio_clear(void){
    xQueueReset(audio_evt_queue_in);
}