#include <jescore.h>
#include <jes_err.h>
#include "dsp_frx.h"
#include "fsm.h"
#include "fsm_jccl.h"
#include "sdcard.h"
#include "audio.h"
#include "wav.h"
#include "adc_base.h"

static e_syserr_t enter_idle(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t enter_record(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t enter_batt(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t enter_sett(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t enter_file(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t exit_idle(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t exit_record(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t exit_batt(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t exit_sett(frx_fsm_t*, frx_fsm_state_t*, void*);
static e_syserr_t exit_file(frx_fsm_t*, frx_fsm_state_t*, void*);
static void routine_idle(frx_fsm_t*, frx_fsm_state_t*, void*);
static void routine_record(frx_fsm_t*, frx_fsm_state_t*, void*);
static void routine_batt(frx_fsm_t*, frx_fsm_state_t*, void*);
static void routine_sett(frx_fsm_t*, frx_fsm_state_t*, void*);
static void routine_file(frx_fsm_t*, frx_fsm_state_t*, void*);
static void fsm_audio_cb(audio_io_t* iobuf);
static void fsm_static_base_cb(fsm_runtime_args_t* rt_args);
static void fsm_static_process_cb(audio_sample_t* buf, uint32_t len, fsm_runtime_args_t* rt_args);
static void fsm_update_samples_to_process(uint32_t samples);

#ifndef FSM_CTRL_JOB_MEM
#define FSM_CTRL_JOB_MEM 2048
#endif
#ifndef FSM_IDLE_JOB_MEM
#define FSM_IDLE_JOB_MEM (2048 * 2)
#endif
#ifndef FSM_RECORDING_JOB_MEM
#define FSM_RECORDING_JOB_MEM (2048 * 5)
#endif
#ifndef FSM_BATTERY_JOB_MEM
#define FSM_BATTERY_JOB_MEM 2048
#endif
#ifndef FSM_SETTINGS_JOB_MEM
#define FSM_SETTINGS_JOB_MEM 2048
#endif
#ifndef FSM_FILE_JOB_MEM
#define FSM_FILE_JOB_MEM (2048 * 2)
#endif

static frx_fsm_t fsm;
static wav_file_t cur_open_wav;
static fsm_runtime_args_t cur_rt_args;
static fsm_runtime_values_t cur_rt_values;
static audio_sample_t audio_buf[AUDIO_BLOCK_SAMPLES*2];

static frx_fsm_state_t states[NUM_FSM_STATES] = {
    {.id=e_fsm_state_idle, .enter=enter_idle, .routine=routine_idle, .exit=exit_idle, .ctx=&cur_rt_args},
    {.id=e_fsm_state_rec, .enter=enter_record, .routine=routine_record, .exit=exit_record, .ctx=&cur_rt_args},
    {.id=e_fsm_state_batt, .enter=enter_batt, .routine=routine_batt, .exit=exit_batt, .ctx=&cur_rt_args},
    {.id=e_fsm_state_sett, .enter=enter_sett, .routine=routine_sett, .exit=exit_sett, .ctx=&cur_rt_args},
    {.id=e_fsm_state_file, .enter=enter_file, .routine=routine_file, .exit=exit_file, .ctx=&cur_rt_args},
    {.id=e_fsm_state_trans, .enter=NULL, .routine=NULL, .exit=NULL, .ctx=&cur_rt_args},
};

static e_syserr_t publish_args(fsm_runtime_args_t* rta){ return frx_fsm_publish_args(&fsm, rta, sizeof(*rta)); }
static e_syserr_t publish_values(fsm_runtime_values_t* rtv){ return frx_fsm_publish_values(&fsm, rtv, sizeof(*rtv)); }

static void zero_audio_vals(fsm_runtime_values_t* rtv, uint8_t nch){
    for(uint8_t i=0; i<nch; i++){
        rtv->msqr.ch[i]=0; rtv->msqr_avg.ch[i]=0; rtv->dbfs.ch[i]=0; rtv->dbfs_avg.ch[i]=0;
    }
}

e_syserr_t fsm_init(void){
    memset(&cur_open_wav, 0, sizeof(cur_open_wav));
    memset(&cur_rt_args, 0, sizeof(cur_rt_args));
    memset(&cur_rt_values, 0, sizeof(cur_rt_values));
    e_syserr_t e = frx_fsm_init(&fsm, states, NUM_FSM_STATES, e_fsm_state_idle, e_fsm_state_trans);
    if(e != e_syserr_none) return e;
    e = frx_fsm_set_snapshot_storage(&fsm, &cur_rt_args, sizeof(cur_rt_args), &cur_rt_values, sizeof(cur_rt_values));
    if(e != e_syserr_none) return e;
    fsm_runtime_args_t rta = {
        .target_state=e_fsm_state_idle, .cur_state=e_fsm_state_trans,
        .data_buf=audio_buf, .data_len=AUDIO_BLOCK_SAMPLES, .samples_to_process=0, .samples_tot=0,
        .wav_file=&cur_open_wav, .sr=AUDIO_SR_DEFAULT, .bps=32, .n_ch=AUDIO_MAX_NUM_CH,
        .sd_mounted=0, .var_args=NULL,
    };
    fsm_runtime_values_t rtv = {.raw_data=rta.data_buf, .len=rta.data_len, .t_transaction=0, .t_system=0, .lipo_mv=0, .plug_mv=0, .sd_free_kb=0, .sd_tot_kb=0};
    zero_audio_vals(&rtv, rta.n_ch);
    cur_rt_args = rta;
    e = frx_fsm_enter_state(&fsm, e_fsm_state_idle);
    if(e != e_syserr_none) return e;
    rta = cur_rt_args;
    publish_values(&rtv);
    e = audio_set_callback(fsm_audio_cb);
    if(e != e_syserr_none) return e;

    jes_err_t je;
    je = jes_register_job(FSM_CTRL_JOB_NAME, FSM_CTRL_JOB_MEM, 1, fsm_job, 0, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    je = jes_register_job(FSM_IDLE_JOB_NAME, FSM_IDLE_JOB_MEM, 1, idle_job, 1, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    je = jes_register_job(FSM_RECORDING_JOB_NAME, FSM_RECORDING_JOB_MEM, 1, record_job, 1, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    je = jes_register_job(FSM_BATTERY_JOB_NAME, FSM_BATTERY_JOB_MEM, 1, batt_job, 1, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    je = jes_register_job(FSM_SETTINGS_JOB_NAME, FSM_SETTINGS_JOB_MEM, 1, sett_job, 1, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    je = jes_register_job(FSM_FILE_JOB_NAME, FSM_FILE_JOB_MEM, 1, file_job, 1, 1);
    if(je != e_err_no_err && je != e_err_duplicate) return (e_syserr_t)je;
    return e_syserr_none;
}

e_syserr_t fsm_init_default(void){ return fsm_init(); }

fsm_runtime_args_t fsm_get_runtime_args(void){ fsm_runtime_args_t rta; memset(&rta,0,sizeof(rta)); frx_fsm_get_args(&fsm,&rta,sizeof(rta)); return rta; }
fsm_runtime_values_t fsm_get_runtime_values(void){ fsm_runtime_values_t rtv; memset(&rtv,0,sizeof(rtv)); frx_fsm_get_values(&fsm,&rtv,sizeof(rtv)); return rtv; }

static void fsm_update_samples_to_process(uint32_t samples){
    fsm_runtime_args_t rta = fsm_get_runtime_args();
    if(rta.cur_state == e_fsm_state_rec){ rta.samples_to_process = samples; publish_args(&rta); }
}

static void fsm_static_base_cb(fsm_runtime_args_t* rt_args){
    fsm_runtime_values_t rtv = fsm_get_runtime_values();
    const uint16_t softclock_max = AUDIO_SR_DEFAULT / (AUDIO_BLOCK_SAMPLES/FSM_UPDATE_SLOW_RATE_S);
    static uint16_t softclock = softclock_max - 1;
    if(++softclock == softclock_max){
        softclock = 0;
        rtv.lipo_mv = adc_base_get_mv(ADC_LIPO_LEVEL_PIN);
        rtv.plug_mv = adc_base_get_mv(ADC_PLUG_DETECT_PIN);
    }
    uint32_t delta = rt_args->samples_tot - rt_args->samples_to_process;
    rtv.t_transaction = rt_args->sr ? (uint32_t)(((float)delta/(float)rt_args->sr) * 1000) : 0;
    rtv.t_system = esp_timer_get_time() / 1000;
    publish_values(&rtv);
}

static void fsm_static_process_cb(audio_sample_t* buf, uint32_t len, fsm_runtime_args_t* rt_args){
    fsm_runtime_values_t rtv = fsm_get_runtime_values();
    rtv.raw_data = buf;
    rtv.len = len;
    rtv.msqr = dsp_frx_samples_to_msqr_32b(buf, len, rt_args->n_ch);
    rtv.msqr_avg = dsp_frx_msqr_rolling_avg(rtv.msqr, rt_args->n_ch);
    rtv.dbfs = dsp_frx_samples_to_dbfs_32b_from_msqr(rtv.msqr, rt_args->n_ch);
    rtv.dbfs_avg = dsp_frx_samples_to_dbfs_32b_from_msqr(rtv.msqr_avg, rt_args->n_ch);
    publish_values(&rtv);
}

static e_syserr_t enter_idle(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; r->cur_state=e_fsm_state_idle; return publish_args(r); }
static e_syserr_t enter_record(frx_fsm_t*, frx_fsm_state_t*, void* ctx){
    fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; cur_open_wav=*r->wav_file; r->wav_file=&cur_open_wav;
    e_syserr_t e=wav_open_for_write(r->wav_file,r->wav_file->filename,r->n_ch,r->sr,r->bps); if(e!=e_syserr_none) return e;
    r->cur_state=e_fsm_state_rec; return publish_args(r);
}
static e_syserr_t enter_batt(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; r->cur_state=e_fsm_state_batt; return publish_args(r); }
static e_syserr_t enter_sett(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; r->cur_state=e_fsm_state_sett; return publish_args(r); }
static e_syserr_t enter_file(frx_fsm_t*, frx_fsm_state_t*, void* ctx){
    fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; r->cur_state=e_fsm_state_file;
    uint32_t totkb=0, freekb=0; e_syserr_t e=sd_get_free_kbytes(&freekb,&totkb); if(e!=e_syserr_none) return e;
    fsm_runtime_values_t rtv=fsm_get_runtime_values(); rtv.sd_free_kb=freekb; rtv.sd_tot_kb=totkb; publish_values(&rtv);
    return publish_args(r);
}
static e_syserr_t exit_idle(frx_fsm_t*, frx_fsm_state_t*, void*){ return e_syserr_none; }
static e_syserr_t exit_record(frx_fsm_t*, frx_fsm_state_t*, void* ctx){
    audio_stop(); jes_delay_job_ms(200);
    fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; e_syserr_t e=wav_close_for_write(r->wav_file); if(e!=e_syserr_none) return e;
    sd_unmnt(); r->sd_mounted=0; r->samples_to_process=0; r->samples_tot=0; memset(r->wav_file,0,sizeof(wav_file_t)); publish_args(r); audio_start(); return e_syserr_none;
}
static e_syserr_t exit_batt(frx_fsm_t*, frx_fsm_state_t*, void*){ return e_syserr_none; }
static e_syserr_t exit_sett(frx_fsm_t*, frx_fsm_state_t*, void*){ return e_syserr_none; }
static e_syserr_t exit_file(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; e_syserr_t e=sd_unmnt(); if(e!=e_syserr_none) return e; r->sd_mounted=0; return publish_args(r); }

e_syserr_t fsm_transition(fsm_state_t from, fsm_state_t to, fsm_runtime_args_t* rta){
    (void)from;
    if(!rta) return e_syserr_null;
    if(to == e_fsm_state_trans) return e_syserr_none;
    rta->target_state = to;
    cur_rt_args = *rta;
    e_syserr_t e = frx_fsm_transition(&fsm, to, 0);
    *rta = cur_rt_args;
    if(e != e_syserr_none){
        jes_throw_error((jes_err_t)e);
    }
    return e;
}

static void routine_idle(frx_fsm_t* f, frx_fsm_state_t*, void* ctx){ fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; audio_io_t* io=(audio_io_t*)frx_fsm_get_block_context(f); if(!io || !io->in) return; fsm_static_process_cb(io->in, io->len, r); fsm_static_base_cb(r); }
static void routine_record(frx_fsm_t* f, frx_fsm_state_t*, void* ctx){
    fsm_runtime_args_t* r=(fsm_runtime_args_t*)ctx; audio_io_t* io=(audio_io_t*)frx_fsm_get_block_context(f); if(!io || !io->in) return;
    fsm_static_process_cb(io->in, io->len, r); fsm_static_base_cb(r); e_syserr_t e=wav_write_samples(r->wav_file, io->in, io->len);
    if(e!=e_syserr_none && e!=e_syserr_oom){ r->samples_to_process=0; uart_unif_writef("record routine died: %d\n\r", e); jes_throw_error((jes_err_t)e_syserr_sdcard_unmnted); }
    r->samples_to_process = (r->samples_to_process > io->len) ? (r->samples_to_process - io->len) : 0;
    fsm_update_samples_to_process(r->samples_to_process);
    if(r->samples_to_process==0) fsm_transition(e_fsm_state_rec, e_fsm_state_idle, r);
}
static void routine_batt(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_static_base_cb((fsm_runtime_args_t*)ctx); }
static void routine_sett(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_static_base_cb((fsm_runtime_args_t*)ctx); }
static void routine_file(frx_fsm_t*, frx_fsm_state_t*, void* ctx){ fsm_static_base_cb((fsm_runtime_args_t*)ctx); }
static void fsm_audio_cb(audio_io_t* iobuf){ frx_fsm_process_current_with_block(&fsm, iobuf); }

#ifdef UNIT_TEST
frx_fsm_t* __fsm_get(void){ return &fsm; }
#endif
