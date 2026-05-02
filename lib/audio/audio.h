#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "syserr.h"
#include <driver/i2s.h>
#include <stdio.h>
#include <stdint.h>

#define AUDIO_SERVER_JOB_NAME   "audio"
#define AUDIO_SERVER_JOB_MEM    (4096)
#define AUDIO_FRAME_LEN         1024
#define AUDIO_I2S_PORT          I2S_NUM_0
#define AUDIO_MAX_NUM_CH        1
#define AUDIO_SR_44100          44100
#define AUDIO_SR_48000          48000
#define AUDIO_SR_96000          96000
#define AUDIO_SR_MAX            AUDIO_SR_96000
#define AUDIO_SR_DEFAULT        AUDIO_SR_48000
#define AUDIO_SR_VALID(sr)      ((sr) == 44100 || (sr) == 48000 || (sr) == 96000)
#define AUDIO_I2S_STARTUP       I2S_EVENT_MAX
#define AUDIO_I2S_RESTART_MS    200     

#define AUDIO_PIN_MEMS_I2S_BCLK 23
#define AUDIO_PIN_MEMS_I2S_WS   4
#define AUDIO_PIN_MEMS_I2S_IN   27

typedef enum{
    I2S_EVENT_STOP = I2S_EVENT_MAX + 1,
    I2S_EVENT_RESTART
}i2s_event_type_ext_t;

typedef int32_t audio_sample_base_t;
typedef float audio_val_base_t;

// typedef struct stereo_sample{
//     int32_t l;
//     int32_t r;
// }stereo_sample_t;

/// @brief Sample description in time. All channels run in parallel.
/// @note Amount of possible channels set by `AUDIO_MAX_NUM_CH`, but
///       the runtime can use less than that if wanted.
typedef union {
    struct {
        audio_sample_base_t ch1;
        #if AUDIO_MAX_NUM_CH > 1
        audio_sample_base_t ch2;
        #endif
        #if AUDIO_MAX_NUM_CH > 2
        audio_sample_base_t ch3;
        #endif
        #if AUDIO_MAX_NUM_CH > 3
        audio_sample_base_t ch4;
        #endif
    }_chx;
    audio_sample_base_t ch[AUDIO_MAX_NUM_CH];
}audio_sample_t;

// typedef struct stereo_value{
//     float l;
//     float r;
// }stereo_value_t;

typedef union {
    struct {
        audio_val_base_t ch1;
        #if AUDIO_MAX_NUM_CH > 1
        audio_val_base_t ch2;
        #endif
        #if AUDIO_MAX_NUM_CH > 2
        audio_val_base_t ch3;
        #endif
        #if AUDIO_MAX_NUM_CH > 3
        audio_val_base_t ch4;
        #endif
    }_chx;
    audio_val_base_t ch[AUDIO_MAX_NUM_CH];
}audio_val_t;


/// @brief Initializes the I2S audio interface.
/// @param sampleRate The sample rate to use.
/// @param bclk The bit clock pin number.
/// @param ws The word select pin number.
/// @param data_rx The data IN pin number.
/// @return Error code indicating success or failure.
e_syserr_t audio_init(uint32_t sampleRate, uint8_t bclk, uint8_t ws, uint8_t data_rx);

/// @brief Initializes the I2S audio interface with default values.
/// @return Error code indicating success or failure.
/// @note Is part of the common signature interface for the init routine.
e_syserr_t audio_init_default(void);

/// @brief 
/// @param  
/// @return 
audio_sample_t* _audio_get_buffer(void);

/// @brief Manages the queue ISR for audio I/O.
/// @param p Pointer to job parameters.
void audio_sampler(void* p);

/// @brief Audio reader function. To be used in FSM state function referenced by "state_func"
/// @param data Pointer to audio input data.
/// @param len Length of total data in samples.
/// @param bps Bits per single channel sample (resolution).
/// @param nch Amount of active channels.
void audio_read(audio_sample_t* data, uint32_t len, uint8_t bps, uint8_t nch);

/// @brief Suspend the audio loop for a short amount of time for state transitions. 
/// @note The length of suspension is set in `AUDIO_I2S_RESTART_MS`
void audio_suspend_short(void);

/// @brief Clear the audio event queue.
void audio_clear(void);

#endif