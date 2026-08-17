#ifndef _FSM_H_
#define _FSM_H_

#include <jescore.h>
#include "syserr.h"
#include "esp_attr.h"
#include "audio.h"
#include "sdcard.h"
#include "wav.h"
#include "fsm_tools.h"

#define FSM_RECORDING_MIN_SPACE (1024 * 10) // 10 MB
#define FSM_JOB_N               5
#define FSM_CTRL_JOB_NAME       "fsm"
#define FSM_IDLE_JOB_NAME       "idle"
#define FSM_RECORDING_JOB_NAME  "record"
#define FSM_BATTERY_JOB_NAME    "batt"
#define FSM_SETTINGS_JOB_NAME   "sett"
#define FSM_FILE_JOB_NAME       "file"
#define FSM_UPDATE_SLOW_RATE_S  8

#ifndef FSM_INTERNAL_VERBOSE
#define FSM_INTERNAL_VERBOSE 0
#endif

#ifdef FR1_DEBUG_PRINT_ENABLE
#ifndef FR1_DEBUG_MSG_FATAL
#define FR1_DEBUG_MSG_FATAL FRX_DEBUG_MSG_FATAL
#endif
#ifndef FR1_DEBUG_MSG_WARN
#define FR1_DEBUG_MSG_WARN FRX_DEBUG_MSG_WARN
#endif
#ifndef FR1_DEBUG_MSG_INFO
#define FR1_DEBUG_MSG_INFO FRX_DEBUG_MSG_INFO
#endif
#define SCOPE_JOB_NAME()   __job_get_job_by_handle(xTaskGetCurrentTaskHandle())->name
#define SCOPE_LOG(fmt, ...) uart_unif_writef("[%s]: " fmt "\n\r", SCOPE_JOB_NAME(), ##__VA_ARGS__)
#define SCOPE_LOG_PJ(pj, fmt, ...) uart_unif_writef("[%s]: " fmt "\n\r", pj->name, ##__VA_ARGS__)
#define SCOPE_LOG_INIT(fmt, ...) uart_unif_writef("[%s]: " fmt "\n\r", "init", ##__VA_ARGS__)
#else
#define SCOPE_JOB_NAME()
#define SCOPE_LOG(fmt, ...)
#define SCOPE_LOG_PJ(pj, fmt, ...)
#define SCOPE_LOG_INIT(fmt, ...)
#endif

typedef enum fsm_state_t{
    e_fsm_state_idle,
    e_fsm_state_rec,
    e_fsm_state_batt,
    e_fsm_state_sett,
    e_fsm_state_file,
    e_fsm_state_trans,
    NUM_FSM_STATES
}fsm_state_t;

typedef struct fsm_runtime_args_t{
    fsm_state_t target_state;
    fsm_state_t cur_state;
    audio_sample_t* data_buf;
    uint32_t data_len;
    uint32_t samples_to_process;
    uint32_t samples_tot;
    wav_file_t* wav_file;
    uint32_t sr;
    uint32_t bps;
    uint8_t n_ch;
    uint8_t sd_mounted;
    void* var_args;
}fsm_runtime_args_t;

typedef struct fsm_runtime_values_t{
    audio_sample_t* raw_data;
    uint32_t len;
    audio_val_t msqr;
    audio_val_t msqr_avg;
    audio_val_t dbfs;
    audio_val_t dbfs_avg;
    uint32_t t_transaction;
    int64_t t_system;
    uint32_t lipo_mv;
    uint32_t plug_mv;
    uint32_t sd_free_kb;
    uint32_t sd_tot_kb;
}fsm_runtime_values_t;

e_syserr_t fsm_init(void);
e_syserr_t fsm_init_default(void);
fsm_runtime_values_t fsm_get_runtime_values(void);
fsm_runtime_args_t fsm_get_runtime_args(void);
e_syserr_t fsm_transition(fsm_state_t from, fsm_state_t to, fsm_runtime_args_t* rta);

#ifdef UNIT_TEST
frx_fsm_t* __fsm_get(void);
#endif

#endif
