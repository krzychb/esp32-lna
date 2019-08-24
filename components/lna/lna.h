// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _LNA_H_
#define _LNA_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/adc.h"


/**
 * @brief Data structure of LNA configuration parameters
 */
typedef struct {
    adc_bits_width_t adc_bits_width;
    adc_atten_t adc_atten;
} lna_config_t;

typedef struct {
    float  mv_b;  // bottom mV input signal value
    int   adc_b;  // bottom ADC reading
    float  mv_t;  // top mV input signal value
    int   adc_t;  // top ADC reading
} adc_mv_cal_t;

esp_err_t adc1_lna_init(lna_config_t* lna_config);
uint32_t adc1_lna_get_value(uint16_t stage1_cycles, uint16_t stage3_cycles);
float lna_adc_to_mv(adc_mv_cal_t cal, adc_bits_width_t i, int adc);


#ifdef __cplusplus
}
#endif

#endif  //_LNA_H_

