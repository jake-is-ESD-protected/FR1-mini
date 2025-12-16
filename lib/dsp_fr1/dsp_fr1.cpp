#include "dsp_fr1.h"

inline float sin_bhaskara_I(float x) {
    x = fmodf(x + M_PI, 2.0f * M_PI) - M_PI; // Wrap to [-π, π]
    float x_pi_minus_x = x * (M_PI - x);
    return (4.0f * x_pi_minus_x) / (M_PI * M_PI - x_pi_minus_x);
}

static inline stereo_value_t dsp_fr1_sample_cleanup(stereo_sample_t data){
    static stereo_value_t dc_values = {.l = 0., .r = 0.};
    data.l >>= 8;
    data.r >>= 8;
    stereo_value_t scaled = {
        .l = (float)data.l * INT24_SCALE,
        .r = (float)data.r * INT24_SCALE};
    dc_values.l += DSP_FR1_DC_FILTER_ALPHA * (scaled.l - dc_values.l);
    dc_values.r += DSP_FR1_DC_FILTER_ALPHA * (scaled.r - dc_values.r);
    scaled.l -= dc_values.l;
    scaled.r -= dc_values.r;
    return scaled;
}

stereo_value_t dsp_fr1_samples_to_msqr_32b(stereo_sample_t* data, uint32_t len){
    stereo_value_t msqr = {0, 0};
    for (size_t i = 0; i < len; i++){
        stereo_value_t s = dsp_fr1_sample_cleanup(data[i]);
        msqr.l += DSP_FR1_SQUARE(s.l);
        msqr.r += DSP_FR1_SQUARE(s.r);
    }
    msqr.l /= len;
    msqr.r /= len;
    return msqr;
}

stereo_value_t dsp_fr1_samples_to_dbfs_32b(stereo_sample_t* data, uint32_t len){
    stereo_value_t msqr = dsp_fr1_samples_to_msqr_32b(data, len);
    stereo_value_t dbfs = {.l = 10*log10f(msqr.l), .r = 10*log10f(msqr.r)};
    return dbfs;
}

stereo_value_t dsp_fr1_samples_to_dbfs_32b_from_msqr(stereo_value_t msqr){
    stereo_value_t dbfs = {.l = 10*log10f(msqr.l), .r = 10*log10f(msqr.r)};
    return dbfs;
}

stereo_value_t dsp_fr1_msqr_rolling_avg(stereo_value_t msqr){
    static stereo_value_t win[DSP_FR1_ROLL_AVG_N];
    static uint16_t roll_idx = 0;
    static uint16_t samples_count = 0;
    static stereo_value_t running_sum = {.l = 0., .r = 0.};
    if (samples_count == DSP_FR1_ROLL_AVG_N) {
        running_sum.l -= win[roll_idx].l;
        running_sum.r -= win[roll_idx].r;
    } else {
        samples_count++;
    }
    win[roll_idx] = msqr;
    running_sum.l += msqr.l;
    running_sum.r += msqr.r;
    if (++roll_idx == DSP_FR1_ROLL_AVG_N) roll_idx = 0;
    stereo_value_t msqr_avg = {
        .l = running_sum.l / (float)samples_count,
        .r = running_sum.r / (float)samples_count
    };
    return msqr_avg;
}