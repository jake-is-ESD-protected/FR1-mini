#include "dsp_fr1.h"

inline float sin_bhaskara_I(float x) {
    x = fmodf(x + M_PI, 2.0f * M_PI) - M_PI; // Wrap to [-π, π]
    float x_pi_minus_x = x * (M_PI - x);
    return (4.0f * x_pi_minus_x) / (M_PI * M_PI - x_pi_minus_x);
}

static inline audio_val_t dsp_fr1_sample_cleanup(audio_sample_t data, uint8_t nch){
    static audio_val_t dc_values = {0};
    audio_val_t scaled;
    for(uint8_t i = 0; i < nch; i++){
        dc_values.ch[i] = 0.;
        data.ch[i] >>= 8;
        scaled.ch[i] = (float)data.ch[i] * INT24_SCALE;
        dc_values.ch[i] += DSP_FR1_DC_FILTER_ALPHA * (scaled.ch[i] - dc_values.ch[i]);
        scaled.ch[i] -= dc_values.ch[i];
    }
    return scaled;
}

audio_val_t dsp_fr1_samples_to_msqr_32b(audio_sample_t* data, uint32_t len, uint8_t nch){
    audio_val_t msqr;
    for(uint8_t i = 0; i < nch; i++){
        msqr.ch[i] = 0.;
    }
    for (size_t i = 0; i < len; i++){
        audio_val_t s = dsp_fr1_sample_cleanup(data[i], nch);
        for(uint8_t j = 0; j < nch; j++){
            msqr.ch[j] += DSP_FR1_SQUARE(s.ch[j]);
        }
    }
    for(uint8_t i = 0; i < nch; i++){
        msqr.ch[i] /= len;
    }
    return msqr;
}

audio_val_t dsp_fr1_samples_to_dbfs_32b(audio_sample_t* data, uint32_t len, uint8_t nch){
    audio_val_t msqr = dsp_fr1_samples_to_msqr_32b(data, len, nch);
    audio_val_t dbfs;
    for(uint8_t i = 0; i < nch; i++){
        dbfs.ch[i] = 10*log10f(msqr.ch[i]);
    }
    return dbfs;
}

audio_val_t dsp_fr1_samples_to_dbfs_32b_from_msqr(audio_val_t msqr, uint8_t nch){
    audio_val_t dbfs;
    for(uint8_t i = 0; i < nch; i++){
        dbfs.ch[i] = 10*log10f(msqr.ch[i]);
    }
    return dbfs;
}

audio_val_t dsp_fr1_msqr_rolling_avg(audio_val_t msqr, uint8_t nch){
    static audio_val_t win[DSP_FR1_ROLL_AVG_N] = {0};
    static uint16_t roll_idx = 0;
    static uint16_t samples_count = 0;
    static audio_val_t running_sum = {0};
    if (samples_count == DSP_FR1_ROLL_AVG_N) {
        for(uint8_t i = 0; i < nch; i++){
            running_sum.ch[i] -= win[roll_idx].ch[i];
        }
    } else {
        samples_count++;
    }
    win[roll_idx] = msqr;
    for(uint8_t i = 0; i < nch; i++){
        running_sum.ch[i] += msqr.ch[i];
    }
    if (++roll_idx == DSP_FR1_ROLL_AVG_N) roll_idx = 0;
    audio_val_t msqr_avg;
    for(uint8_t i = 0; i < nch; i++){
        msqr_avg.ch[i] = running_sum.ch[i] / (float)samples_count;
    }
    return msqr_avg;
}