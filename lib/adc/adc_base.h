#ifndef _ADC_BASE_H_
#define _ADC_BASE_H_

#include "syserr.h"

#define ADC_LIPO_LEVEL_PIN (uint8_t)35
#define ADC_PLUG_DETECT_PIN (uint8_t)32
#define ADC_BASE_JOB_NAME "adc"
#define ADC_LIPO_LVL_MIN_MV 3600    // estimation
#define ADC_LIPO_LVL_MAX_MV 3900    // estimation

/// @brief 
/// @param pin 
/// @return 
e_syserr_t adc_base_init(uint8_t pin);

/// @brief 
/// @return 
e_syserr_t adc_base_init_default(void);

/// @brief 
/// @param pin 
/// @return 
uint32_t adc_base_get_mv(uint8_t pin);

/// @brief 
/// @param p 
void adc_base_job(void* p);

#endif // _ADC_BASE_H_