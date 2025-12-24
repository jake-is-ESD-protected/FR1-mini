#ifndef _FSM_H_
#define _FSM_H_

#include <jescore.h>
#include "syserr.h"
#include "esp_attr.h"
#include "sdcard.h"
#include "wav.h"
#include "freertos/semphr.h"

#define UNIF_UART_WRITE_BUF_SIZE 128         // this overwrites a macro in jescore
#define FSM_RECORDING_MIN_SPACE (1024 * 10) // 10 MB


#define FSM_JOB_N               5
#define FSM_CTRL_JOB_NAME       "fsm" // not counted 
#define FSM_IDLE_JOB_NAME       "idle"
#define FSM_RECORDING_JOB_NAME  "record"
#define FSM_BATTERY_JOB_NAME    "batt"
#define FSM_SETTINGS_JOB_NAME   "sett"
#define FSM_FILE_JOB_NAME       "file"

#define FSM_UPDATE_SLOW_RATE_S  8 // every 8 seconds, the FSM updates "slow" values

#ifndef FSM_INTERNAL_VERBOSE
#define FSM_INTERNAL_VERBOSE 0
#endif // FSM_INTERNAL_VERBOSE

#ifdef FR1_DEBUG_PRINT_ENABLE
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

/// @brief Enumeration of state types.
typedef enum fsm_state_t{
    e_fsm_state_idle,
    e_fsm_state_rec,
    e_fsm_state_batt,
    e_fsm_state_sett,
    e_fsm_state_file,
    e_fsm_state_trans,
    NUM_FSM_STATES
}fsm_state_t;

/// @brief Runtime arguments for state routine.
typedef struct fsm_runtime_args_t{
    fsm_state_t target_state;   // what the rta wants
    fsm_state_t cur_state;      // what is currently in the fsm
    stereo_sample_t* data_buf;
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

/// @brief Runtime values obtained from state routine.
typedef struct fsm_runtime_values_t{
    stereo_sample_t* raw_data;
    uint32_t len;
    stereo_value_t msqr;
    stereo_value_t msqr_avg;
    stereo_value_t dbfs;
    stereo_value_t dbfs_avg;
    uint32_t t_transaction;
    int64_t t_system;
    uint32_t lipo_mv;
    uint32_t plug_mv;
    uint32_t sd_free_kb;
    uint32_t sd_tot_kb;
}fsm_runtime_values_t;

/// @brief State execution routine type.
typedef void (*state_func_t)(fsm_runtime_args_t* rta);
/// @brief State enter routine type.
typedef e_syserr_t (*state_enter_t)(fsm_runtime_args_t* rta);
/// @brief State exit routine type.
typedef e_syserr_t (*state_exit_t)(fsm_runtime_args_t* rta);

/// @brief State abstraction.
typedef struct fsm_state_struct_t{
    fsm_state_t name;
    state_enter_t enter;
    state_func_t routine;
    state_exit_t exit;
    fsm_runtime_args_t rt_args;
    fsm_runtime_values_t rt_vals;
    SemaphoreHandle_t lock;
}fsm_state_struct_t;

/// @brief FSM abstraction.
typedef struct fsm_t{
    fsm_state_t cur_state;
    fsm_state_struct_t states[NUM_FSM_STATES];
    job_struct_t* audio_job_handle;
    wav_file_t cur_open_wav;
}fsm_t;

/// @brief Initialize the FSM.
/// @return FR1 error code.
e_syserr_t fsm_init(void);

/// @brief Initialize the FSM with default parameters.
/// @return FR1 error code.
/// @note Is part of the common signature interface for the init routine.
e_syserr_t fsm_init_default(void);

/// @brief Get the current runtime values.
/// @return Runtime values (computed values from state routine).
fsm_runtime_values_t fsm_get_runtime_values(void);

/// @brief Get the current runtime arguments.
/// @return Runtime arguments (FSM context).
fsm_runtime_args_t fsm_get_runtime_args(void);

/// @brief Call the enter routine of the state s.
/// @param s State to enter.
/// @param rta Pointer to runtime arguments. Passed onto routine.
/// @return FR1 error code.
e_syserr_t fsm_enter_state(fsm_state_t s, fsm_runtime_args_t* rta);

/// @brief Call the exit routine of the state s.
/// @param s State to enter.
/// @param rta Pointer to runtime arguments. Used for cleanup.
/// @return FR1 error code.
e_syserr_t fsm_exit_state(fsm_state_t s, fsm_runtime_args_t* rta);

/// @brief Call the execution routine of the state s.
/// @param s State to execute.
/// @param rta Pointer to runtime arguments. Used for runtime prameters.
void fsm_routine_state(fsm_state_t s, fsm_runtime_args_t* rta);

/// @brief Transition from one state to another.
/// @param from State to transition from.
/// @param to State to transition to.
/// @param rta Runtime arguments.
/// @return FR1 error code.
e_syserr_t fsm_transition(fsm_state_t from, fsm_state_t to, fsm_runtime_args_t* rta);

#ifdef UNIT_TEST
extern QueueHandle_t lock_interface;

/// @brief Get the static fsm struct pointer.
/// @return FSM struct pointer.
/// @note DO NOT USE THIS OUTISDE OF UNIT TESTS!
fsm_t* __fsm_get(void);
#endif

#endif