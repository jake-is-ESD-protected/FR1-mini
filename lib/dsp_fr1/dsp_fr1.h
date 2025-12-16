#ifndef _DSP_FR1_H_
#define _DSP_FR1_H_

#include <math.h>
#include "audio.h"

#define DSP_FR1_SQUARE(x) (x * x)

/// @brief Fast sine approximation based on Bhaskara I algorithm.
/// @param x Wrapped or unwrapped phase.
/// @return Sine approximation.
inline float sin_bhaskara_I(float x);

stereo_value_t dsp_fr1_samples_to_msqr_32b(stereo_sample_t* data, uint32_t len);

stereo_value_t dsp_fr1_samples_to_dbfs_32b(stereo_sample_t* data, uint32_t len);

stereo_value_t dsp_fr1_samples_to_dbfs_32b_from_msqr(stereo_value_t msqr);

#endif