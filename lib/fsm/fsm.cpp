#include <jescore.h>
#include <jes_err.h>
#include "dsp_fr1.h"
#include "fsm.h"
#include "fsm_jccl.h"
#include "sdcard.h"
#include <driver/i2s.h>
#include "audio.h"
#include "wav.h"
#include "adc_base.h"

/// @brief Callback for fetching basic system data. 
/// @param rta Pointer to runtime arguments. Passed onto routine.
static inline void fsm_static_base_cb(fsm_runtime_args_t* rta);

/// @brief Callback for audio processing.
/// @param buf Audio buffer.
/// @param len Length of buffer.
/// @param rta Pointer to runtime arguments. Passed onto routine.
static inline  void fsm_static_process_cb(stereo_sample_t* buf, uint32_t len, fsm_runtime_args_t* rta);

/// @brief Enter routine for idle state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_enter_idle(fsm_runtime_args_t* rta);

/// @brief Enter routine for record state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_enter_record(fsm_runtime_args_t* rta);

/// @brief Enter routine for settings state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_enter_sett(fsm_runtime_args_t* rta);

/// @brief Exit routine for idle state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_exit_idle(fsm_runtime_args_t* rta);

/// @brief Exit routine for record state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_exit_record(fsm_runtime_args_t* rta);

/// @brief Exit routine for settings state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
static inline e_syserr_t fsm_exit_sett(fsm_runtime_args_t* rta);

/// @brief Routine to loop over while in idle state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @note This function is intended to be called as
/// `state_func_t` function pointer in the **audio** loop.
static inline void fsm_idle(fsm_runtime_args_t* rta);

/// @brief Routine to loop over while in record state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @note This function is intended to be called as
/// `state_func_t` function pointer in the **audio** loop.
static inline void fsm_record(fsm_runtime_args_t* rta);

/// @brief Routine to loop over while in settings state.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @note This function is intended to be called as
/// `state_func_t` function pointer in the **audio** loop.
static inline void fsm_sett(fsm_runtime_args_t* rta);

/// @brief Update the runtime args for the global buffer.
/// @param rta Current runtime args.
/// @note Can be obtained from outside with `fsm_get_runtime_args()`.
static inline void fsm_update_runtime_args(fsm_runtime_args_t* rta);

/// @brief Update the runtime values for the global buffer.
/// @param rta Current runtime values.
/// @note Can be obtained from outside with `fsm_get_runtime_values()`.
static inline void fsm_update_runtime_values(fsm_runtime_values_t* rtv);

static fsm_state_struct_t __idle = {
    .name = e_fsm_state_idle,
    .enter = fsm_enter_idle,
    .routine = fsm_idle,
    .exit = fsm_exit_idle,
    .rt_args = {},
    .rt_vals = {},
    .lock = NULL
};

static fsm_state_struct_t __record = {
    .name = e_fsm_state_rec,
    .enter = fsm_enter_record,
    .routine = fsm_record,
    .exit = fsm_exit_record,
    .rt_args = {},
    .rt_vals = {},
    .lock = NULL
};

static fsm_state_struct_t __sett = {
    .name = e_fsm_state_sett,
    .enter = fsm_enter_sett,
    .routine = fsm_sett,
    .exit = fsm_exit_sett,
    .rt_args = {},
    .rt_vals = {},
    .lock = NULL
};

static fsm_state_struct_t __trans = {
    .name = e_fsm_state_trans,
    .enter = NULL,
    .routine = NULL,
    .exit = NULL,
    .rt_args = {},
    .rt_vals = {},
    .lock = NULL
};

static fsm_t fsm = {
    .cur_state = e_fsm_state_idle,
    .states = {__idle, __record, __sett, __trans},
    .audio_job_handle = NULL,
    // .cur_open_wav = WAV_DEFAULT_HEADER_STRUCT;
};

static stereo_sample_t audio_buf[AUDIO_FRAME_LEN*2];
static fsm_runtime_args_t cur_rt_args;
static fsm_runtime_values_t cur_rt_values;
#ifdef UNIT_TEST
QueueHandle_t lock_interface;
#else
static QueueHandle_t lock_interface;
#endif // UNIT_TEST


e_syserr_t fsm_init(void){
    job_struct_t* audio_job = __job_get_job_by_name(AUDIO_SERVER_JOB_NAME);
    if(audio_job == NULL) return e_syserr_null;
    fsm.audio_job_handle = audio_job;
    memset(&fsm.cur_open_wav, 0, sizeof(wav_file_t));
    for(uint8_t i = 0; i < NUM_FSM_STATES; i++){
        fsm.states[i].lock = xSemaphoreCreateMutex();
        if(fsm.states[i].lock == NULL) return e_syserr_null;
    }
    fsm_runtime_args_t rta = {
        .target_state = e_fsm_state_idle,
        .cur_state = e_fsm_state_trans,
        .data_buf = audio_buf,
        .data_len = AUDIO_FRAME_LEN,
        .samples_to_process = 0,
        .samples_tot = 0,
        .wav_file = &fsm.cur_open_wav,
        .sr = 0,
        .bps = 0,
        .n_ch = 0,
        .sd_mounted = 0,
        .var_args = NULL,
    };
    fsm_runtime_values_t rtv = {
        .raw_data = rta.data_buf,
        .len = rta.data_len,
        .msqr = {.l = 0., .r = 0.},
        .msqr_avg = {.l = 0., .r = 0.},
        .dbfs = {.l = 0., .r = 0.},
        .dbfs_avg = {.l = 0., .r = 0.},
        .t_transaction = 0,
        .t_system = 0,
        .lipo_mv = 0,
        .plug_mv = 0,
    };
    lock_interface = xSemaphoreCreateMutex();
    e_syserr_t e = fsm_enter_idle(&rta);
    if(e != e_syserr_none) { return e; }
    fsm_update_runtime_args(&rta);
    fsm_update_runtime_values(&rtv);

    jes_err_t je; 
    je = jes_register_job(FSM_CTRL_JOB_NAME, 2048, 1, fsm_job, 0);
    if(je != e_err_no_err) { return (e_syserr_t)je;}
    je = jes_register_job(FSM_IDLE_JOB_NAME, 2048*2, 1, idle_job, 1);
    if(je != e_err_no_err) { return (e_syserr_t)je;}
    je = jes_register_job(FSM_RECORDING_JOB_NAME, 2048*5, 1, record_job, 1);
    if(je != e_err_no_err) { return (e_syserr_t)je;}
    je = jes_register_job(FSM_SETTINGS_JOB_NAME, 2048, 1, sett_job, 1);
    if(je != e_err_no_err) { return (e_syserr_t)je;}
    return e_syserr_none;
}

e_syserr_t fsm_init_default(void){
    return fsm_init();
}

static inline void fsm_update_runtime_args(fsm_runtime_args_t* rta){
    xSemaphoreTake(lock_interface, portMAX_DELAY);
    cur_rt_args = *rta;
    xSemaphoreGive(lock_interface);
}

fsm_runtime_args_t fsm_get_runtime_args(void){
    xSemaphoreTake(lock_interface, portMAX_DELAY);
    fsm_runtime_args_t rta = cur_rt_args;
    xSemaphoreGive(lock_interface);
    return rta;
}

static inline void fsm_update_runtime_values(fsm_runtime_values_t *rtv){
    xSemaphoreTake(lock_interface, portMAX_DELAY);
    cur_rt_values = *rtv;
    xSemaphoreGive(lock_interface);
}

fsm_runtime_values_t fsm_get_runtime_values(void){
    xSemaphoreTake(lock_interface, portMAX_DELAY);
    fsm_runtime_values_t rtv = cur_rt_values;
    xSemaphoreGive(lock_interface);
    return rtv;
}

static inline void fsm_update_samples_to_process(uint32_t samples){
    if(cur_rt_args.cur_state == e_fsm_state_rec){
        xSemaphoreTake(lock_interface, portMAX_DELAY);
        cur_rt_args.samples_to_process = samples;
        xSemaphoreGive(lock_interface);
    }
}

static inline void fsm_static_base_cb(fsm_runtime_args_t* rt_args){
    fsm_runtime_values_t rtv = fsm_get_runtime_values();
    // hardcoded
    const uint16_t softclock_max = AUDIO_SR_DEFAULT / (AUDIO_FRAME_LEN/FSM_UPDATE_SLOW_RATE_S); 
    static uint16_t softclock = softclock_max - 1;
    static uint16_t adc_lipo_last_mv;
    static uint16_t adc_plug_last_mv;
    if(++softclock == softclock_max){
        softclock = 0;
        adc_lipo_last_mv = rtv.lipo_mv = adc_base_get_mv(ADC_LIPO_LEVEL_PIN);
        adc_plug_last_mv = rtv.plug_mv = adc_base_get_mv(ADC_PLUG_DETECT_PIN);
    }
    uint32_t delta = rt_args->samples_tot - rt_args->samples_to_process;
    rtv.t_transaction = (uint32_t)(((float)delta/(float)rt_args->sr) * 1000);
    rtv.t_system = esp_timer_get_time() / 1000;
    fsm_update_runtime_values(&rtv);
}

static inline  void fsm_static_process_cb(stereo_sample_t* buf, uint32_t len, fsm_runtime_args_t* rt_args){
    fsm_runtime_values_t rtv = fsm_get_runtime_values();
    rtv.raw_data = buf;
    rtv.len = AUDIO_FRAME_LEN;
    rtv.msqr = dsp_fr1_samples_to_msqr_32b(rtv.raw_data, rtv.len);
    rtv.msqr_avg = dsp_fr1_msqr_rolling_avg(rtv.msqr);
    rtv.dbfs = dsp_fr1_samples_to_dbfs_32b_from_msqr(rtv.msqr);
    rtv.dbfs_avg = dsp_fr1_samples_to_dbfs_32b_from_msqr(rtv.msqr_avg);
    fsm_update_runtime_values(&rtv);
}

static inline e_syserr_t fsm_enter_idle(fsm_runtime_args_t* rta){
    fsm_state_struct_t* pstate = &fsm.states[e_fsm_state_idle];
    // xSemaphoreTake(pstate->lock, portMAX_DELAY); // TODO: lock usage
    rta->cur_state = e_fsm_state_idle;
    pstate->rt_args = *rta;
    jes_err_t je = __job_set_param(pstate, 
                                   fsm.audio_job_handle);
    if(je != e_err_no_err) return (e_syserr_t)je;
    fsm.cur_state = e_fsm_state_idle;
    return e_syserr_none;
}

static inline e_syserr_t fsm_enter_record(fsm_runtime_args_t* rta){
    fsm_state_struct_t* pstate = &fsm.states[e_fsm_state_rec];
    fsm.cur_open_wav = *rta->wav_file; // copy wav struct
    rta->wav_file = &fsm.cur_open_wav; // map ref to fsm's instance
    e_syserr_t e = wav_open_for_write(rta->wav_file, 
                                      rta->wav_file->filename, 
                                      rta->n_ch, 
                                      rta->sr, 
                                      rta->bps);
    if(e != e_syserr_none) return e;
    rta->cur_state = e_fsm_state_rec;
    pstate->rt_args = *rta;
    jes_err_t je = __job_set_param(pstate, 
                                   fsm.audio_job_handle);
    if(je != e_err_no_err) return (e_syserr_t)je;
    fsm.cur_state = e_fsm_state_rec;
    return e_syserr_none;
}

static inline e_syserr_t fsm_enter_sett(fsm_runtime_args_t* rta){
    fsm_state_struct_t* pstate = &fsm.states[e_fsm_state_sett];
    rta->cur_state = e_fsm_state_sett;
    pstate->rt_args = *rta;
    jes_err_t je = __job_set_param(pstate, 
                                   fsm.audio_job_handle);
    if(je != e_err_no_err) return (e_syserr_t)je;
    fsm.cur_state = e_fsm_state_sett;
    return e_syserr_none;
}

e_syserr_t fsm_enter_state(fsm_state_t s, fsm_runtime_args_t* rta){
    if(s == e_fsm_state_trans) return e_syserr_none;
    rta->target_state = s;
    return fsm.states[s].enter(rta);
}

static inline e_syserr_t fsm_exit_idle(fsm_runtime_args_t* rta){
    fsm.cur_state = e_fsm_state_trans;
    return e_syserr_none;
}

static inline e_syserr_t fsm_exit_record(fsm_runtime_args_t* rta){
    e_syserr_t e = wav_close_for_write(rta->wav_file);
    if(e != e_syserr_none) {
        #if FSM_INTERNAL_VERBOSE == 1
        SCOPE_LOG("err: %d, unable to close wav file %x", e, rta->wav_file->file);
        #endif
        return e; 
    }
    // memset(rta->wav_file, 0, sizeof(wav_file_t)); /// TODO:
    // e = sd_unmnt();
    // if(e != e_syserr_none) {
    //     #if FSM_INTERNAL_VERBOSE == 1
    //     SCOPE_LOG("err: %d, unable to unmount SD card", e);
    //     #endif
    //     return e; 
    // }
    rta->samples_to_process = 0;
    rta->samples_tot = 0;
    fsm.cur_state = e_fsm_state_trans;
    return e_syserr_none;
}

static inline e_syserr_t fsm_exit_sett(fsm_runtime_args_t* rta){
    fsm.cur_state = e_fsm_state_trans;
    return e_syserr_none;
}

e_syserr_t fsm_exit_state(fsm_state_t s, fsm_runtime_args_t* rta){
    if(s == e_fsm_state_trans) return e_syserr_none;
    return fsm.states[s].exit(rta);
}

static inline void fsm_idle(fsm_runtime_args_t* rta){
    static uint8_t frame_pos = 0;
    audio_read(&audio_buf[rta->data_len*(frame_pos)], rta->data_len);
    fsm_static_process_cb(&audio_buf[rta->data_len*(!frame_pos)], rta->data_len, rta);
    fsm_static_base_cb(rta);
    frame_pos = !frame_pos;
}

static inline void fsm_record(fsm_runtime_args_t* rta) {
    e_syserr_t e;
    static uint8_t frame_pos = 0;
    static uint8_t test = 0;
    audio_read(&audio_buf[rta->data_len * frame_pos], rta->data_len);
    fsm_static_process_cb(&audio_buf[rta->data_len*(!frame_pos)], rta->data_len, rta);
    fsm_static_base_cb(rta);
    e = wav_write_samples(rta->wav_file, &audio_buf[rta->data_len * (!frame_pos)], rta->data_len);
    if(e != e_syserr_none && e != e_syserr_oom){
        rta->samples_to_process = 0;
        // this is an assumption:
        uart_unif_writef("record routine died: %d\n\r", e);
        jes_throw_error((jes_err_t)e_syserr_sdcard_unmnted);
        // sd_unmnt();
    }
    if (rta->samples_to_process > rta->data_len) {
        rta->samples_to_process -= rta->data_len;
    } else {
        rta->samples_to_process = 0;
    }
    frame_pos = !frame_pos;
    fsm_update_samples_to_process(rta->samples_to_process);
    if (rta->samples_to_process == 0) {
        fsm_transition(e_fsm_state_rec, e_fsm_state_idle, rta);
    }
}

static inline void fsm_sett(fsm_runtime_args_t* rta){
    fsm_static_base_cb(rta);
}

void fsm_routine_state(fsm_state_t s, fsm_runtime_args_t* rta){
    if(s == e_fsm_state_trans) return;
    fsm.states[s].routine(rta);
}

e_syserr_t fsm_transition(fsm_state_t from, fsm_state_t to, fsm_runtime_args_t* rta){
    audio_suspend_short();
    e_syserr_t e;
    #if FSM_INTERNAL_VERBOSE == 1
    SCOPE_LOG("Exiting <%d>!", from);
    #endif
    e = fsm_exit_state(from, rta);
    if(e != e_syserr_none){
        #if FSM_INTERNAL_VERBOSE == 1
        SCOPE_LOG("Could not exit <%d>!", from);
        #endif
        jes_throw_error((jes_err_t)e);
        return e;
    }
    fsm_update_runtime_args(rta);
    #if FSM_INTERNAL_VERBOSE == 1
    SCOPE_LOG("Entering <%d>!", to);
    #endif
    e = fsm_enter_state(to, rta);
    if(e != e_syserr_none){
        #if FSM_INTERNAL_VERBOSE == 1
        SCOPE_LOG("Could not enter <%d>!", to);
        #endif
        jes_throw_error((jes_err_t)e);
        return e;
    }
    fsm_update_runtime_args(rta);
    return e_syserr_none;
}

fsm_t* __fsm_get(void){
    return &fsm;
}
