#ifndef _DSP_FR1_H_
#define _DSP_FR1_H_

#include <math.h>
#include "audio.h"

#define DSP_FR1_SQUARE(x) ((x) * (x))
#define DSP_FR1_MIC_SENS -26
#define DSP_FR1_DBFS_TO_SPL(x) ((x) + (-1)*DSP_FR1_MIC_SENS + 94)

#define INT24_SCALE (1.0f / 8388608.0f)
#define DSP_FR1_DC_FILTER_ALPHA (0.001)

#define DSP_FR1_ROLL_AVG_N 10

/// @brief Fast sine approximation based on Bhaskara I algorithm.
/// @param x Wrapped or unwrapped phase.
/// @return Sine approximation.
inline float sin_bhaskara_I(float x);

/// @brief 
/// @param data 
/// @param len 
/// @return 
stereo_value_t dsp_fr1_samples_to_msqr_32b(stereo_sample_t* data, uint32_t len);

/// @brief 
/// @param data 
/// @param len 
/// @return 
stereo_value_t dsp_fr1_samples_to_dbfs_32b(stereo_sample_t* data, uint32_t len);

/// @brief 
/// @param msqr 
/// @return 
stereo_value_t dsp_fr1_samples_to_dbfs_32b_from_msqr(stereo_value_t msqr);

/// @brief 
/// @param msqr 
/// @return 
stereo_value_t dsp_fr1_msqr_rolling_avg(stereo_value_t msqr);

#endif