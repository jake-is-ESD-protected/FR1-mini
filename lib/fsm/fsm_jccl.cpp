#include "fsm.h"
#include "fsm_jccl.h"
#include "jescore.h"
#include <Arduino.h>

const char fsm_jccl_jobs[FSM_JOB_N][8] = {
    FSM_IDLE_JOB_NAME,
    FSM_RECORDING_JOB_NAME,
    FSM_SETTINGS_JOB_NAME
};

void fsm_job(void* p){
    job_struct_t* pj = (job_struct_t*)p;
    char* args = jes_job_get_args();
    char* arg = strtok(args, " ");
    if(!arg){
        SCOPE_LOG_PJ(pj, "Usage: fsm <state> [args]");
        return;
    }
    if(strcmp("state", arg) == 0){
        fsm_runtime_args_t rta = fsm_get_runtime_args();
        SCOPE_LOG_PJ(pj, "Current state: %d", rta.cur_state);
        return;
    }
    for(uint8_t i = 0; i < FSM_JOB_N; i++){
        if(strcmp(arg, fsm_jccl_jobs[i]) == 0){
            job_struct_t* pj = __job_get_job_by_name(arg);
            if(!pj){
                break;
            }
            char* transfer_args = arg + strlen(arg) + 1;
            if(transfer_args){
                jes_err_t e = __job_set_args(arg + strlen(arg) + 1, pj);
                if(e != e_err_no_err){
                    SCOPE_LOG_PJ(pj, "Could not transfer args <%s> to state <%s>", 
                              transfer_args, arg);
                    jes_throw_error(e_err_unknown_job);
                    return;
                }
            }
            SCOPE_LOG_PJ(pj, "Notifying state jccl <%s>", arg);
            jes_notify_job(arg, NULL);
            jes_delay_job_ms(50); // hang around some time to let the other JCCLs print
            return;
        }
    }
    SCOPE_LOG_PJ(pj, "State <%s> not registered!", arg);
    jes_throw_error(e_err_unknown_job);
}

void idle_job(void* p){
    job_struct_t* pj = (job_struct_t*)p;
    fsm_runtime_args_t rta;
    pj->role = e_role_core;
    while(1){
        jes_wait_for_notification();
        char* args = jes_job_get_args();
        char* arg = strtok(args, " ");
        rta = fsm_get_runtime_args();
        e_syserr_t e;

        if(rta.cur_state == e_fsm_state_rec){
            if(arg == NULL){
                SCOPE_LOG_PJ(pj, "Device is still recording. Use 'idle home' to force idle state.");
                continue;
            }
            else if(strcmp(arg, "home") == 0){
                SCOPE_LOG_PJ(pj, "Force-stop recording...");
            }
            else{
                SCOPE_LOG_PJ(pj, "Unknown argument for idle.");
                jes_throw_error((jes_err_t)e_syserr_param);
                continue;
            }
        }
        FSM_JCCL_TRANSITION_OR_CONTINUE(rta.cur_state, e_fsm_state_idle, &rta);
    }
}

void record_job(void* p){
    fsm_runtime_args_t rta;
    job_struct_t* pj = (job_struct_t*)p;
    pj->role = e_role_core;
    while(1){
        jes_wait_for_notification();
        char* args = jes_job_get_args();
        char* arg = strtok(args, " ");
        rta = fsm_get_runtime_args();
        e_syserr_t e;

        if(arg == NULL){
            SCOPE_LOG_PJ(pj, "Usage: record [start, stop] (-s samples).");
            continue;
        }
        
        // check if SD can be reached
        if(!sd_is_mounted()){
            SCOPE_LOG_PJ(pj, "Cannot record, SD not mounted.");
            jes_throw_error((jes_err_t)e_syserr_sdcard_unmnted);
            continue;
        }
        
        // check if enough space is available on SD card
        uint32_t free_kbytes = 0;
        uint32_t all_kbytes = 0;
        if(sd_get_free_kbytes(&free_kbytes, &all_kbytes) != e_syserr_none){
            SCOPE_LOG_PJ(pj, "Cannot record, free space can't be identified.");
            jes_throw_error((jes_err_t)e_syserr_file_generic);
            continue;
        }
        if(free_kbytes < FSM_RECORDING_MIN_SPACE) {
            SCOPE_LOG_PJ(pj, "Cannot record, card is full.");
            jes_throw_error((jes_err_t)e_syserr_oom);
            continue;
        }
        uint32_t max_samples = free_kbytes * 1024 * sizeof(stereo_sample_t); 
        
        if(strcmp(arg, "start") == 0){
            if(rta.cur_state == e_fsm_state_rec){
                SCOPE_LOG_PJ(pj, "Already recording.");
                continue;
            }
            char* flag = strtok(NULL, " ");
            if (flag){
                char* value = strtok(NULL, " ");
                if (!value){
                    SCOPE_LOG_PJ(pj, "Flag given but value is missing!");
                    jes_throw_error((jes_err_t)e_syserr_param);
                    continue;
                };
                if (strcmp(flag, "-s") == 0) {
                    uint32_t n = atoi(value);
                    if (n == 0 || n > max_samples){
                        SCOPE_LOG_PJ(pj, "Can't record this amount of samples!");
                        jes_throw_error((jes_err_t)e_syserr_param);
                        continue;
                    }
                    max_samples = n;
                }
            }
            SCOPE_LOG_PJ(pj, "Starting recording of %d samples...", max_samples);
            wav_file_t wav = {
                /*.filename =*/SDCARD_BASE_PATH "/" SDCARD_DEFAULT_FNAME_WAV,
                /*.file =*/NULL,
                /*.header =*/{0},
                /*.samples_transfered =*/0
            };
            e = sd_get_unique_fname(wav.filename);
            
            if(e != e_syserr_none){
                SCOPE_LOG_PJ(pj, "Could not create filename.");
                jes_throw_error((jes_err_t)e);
                continue;
            }
            SCOPE_LOG_PJ(pj, "Preparing <%s>", wav.filename);
            
            /* The wav-struct referenced by this rta struct is destroyed
            after the JCCL finishes the request. It is copied to the fsm's
            own wav instance in the "enter" function if this state. 
            */
            rta.samples_to_process = max_samples;
            rta.samples_tot = max_samples;
            rta.wav_file = &wav;
            rta.sr = AUDIO_SR_DEFAULT;
            rta.bps = 32;
            rta.n_ch = 2;

            FSM_JCCL_TRANSITION_OR_CONTINUE(rta.cur_state, e_fsm_state_rec, &rta);
        }
        else if(strcmp(arg, "stop") == 0){
            if(rta.cur_state != e_fsm_state_rec){
                SCOPE_LOG_PJ(pj, "No running recording found.");
                continue;
            }
            SCOPE_LOG_PJ(pj, "Stopping recording...");
            FSM_JCCL_TRANSITION_OR_CONTINUE(e_fsm_state_rec, e_fsm_state_idle, &rta);
        }
        else{
            SCOPE_LOG_PJ(pj, "Unknown argument for record.");
            jes_throw_error((jes_err_t)e_syserr_param);
            continue;
        }
    }
}

void sett_job(void* p){
    job_struct_t* pj = (job_struct_t*)p;
    fsm_runtime_args_t rta;
    pj->role = e_role_core;
    while(1){
        jes_wait_for_notification();
        char* args = jes_job_get_args();
        char* arg = strtok(args, " ");
        rta = fsm_get_runtime_args();
        e_syserr_t e;
        FSM_JCCL_TRANSITION_OR_CONTINUE(rta.cur_state, e_fsm_state_sett, &rta);
    }
}