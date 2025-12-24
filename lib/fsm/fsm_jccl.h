#ifndef _FSM_JCCL_H_
#define _FSM_JCCL_H_

#include "fsm.h"

extern const char fsm_jccl_jobs[FSM_JOB_N][8];

#define FSM_JCCL_TRANSITION_OR_CONTINUE(from, to, prta) \
    do { \
        e_syserr_t syserr; \
        syserr = fsm_transition(from, to, prta); \
        if(syserr != e_syserr_none) continue; \
    } while(0)


/// @brief FSM job. CLI callable. Switches to given state with args.
/// @param p Pointer to job parameters (set by jescore).
void fsm_job(void* p);

/// @brief Homing job. Returns to the idle state transition.
/// @param p Pointer to job parameters (set by jescore).
void idle_job(void* p);

/// @brief Recording job. Handles the recording state transition.
/// @param p Pointer to job parameters (set by jescore).
void record_job(void* p);

/// @brief Battery job. Handles the battery state transition.
/// @param p Pointer to job parameters (set by jescore).
void batt_job(void* p);

/// @brief Settings job. Handles the settings state transition.
/// @param p Pointer to job parameters (set by jescore).
void sett_job(void* p);

/// @brief File viewer job. Handles the file state transition.
/// @param p Pointer to job parameters (set by jescore).
void file_job(void* p);

#endif // _FSM_JCCL_H_