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

#include "esp_log.h"

#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "lna.h"

#define SAR1_CLK_DIV    (2)

static const char* TAG = "LNA Driver";


static esp_err_t adc1_pad_init(adc1_channel_t channel)
{
    esp_err_t result;
    gpio_num_t gpio_num = 0;
    result  = adc1_pad_get_io_num(channel, &gpio_num);
    result |= rtc_gpio_init(gpio_num);
    result |= rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_DISABLED);
    result |= gpio_set_pull_mode(gpio_num, GPIO_FLOATING);
    if (result != ESP_OK ) {
        ESP_LOGE(TAG, "Failure initializing ADC channel %d", channel);
    }
    return result;
}


esp_err_t adc1_lna_init(lna_config_t* lna_config)
{
    esp_err_t result;
    result  = adc1_pad_init(ADC1_CHANNEL_0);  // SENSOR_VP   / GPIO36
    result |= adc1_pad_init(ADC1_CHANNEL_1);  // SENSOR_CAPP / GPIO37
    result |= adc1_pad_init(ADC1_CHANNEL_2);  // SENSOR_CAPN / GPIO38
    result |= adc1_pad_init(ADC1_CHANNEL_3);  // SENSOR_VN   / GPIO39

    // Do we need to configure attenuation for all four channels?
    result |= adc1_config_channel_atten(ADC1_CHANNEL_0, lna_config->adc_atten);
    result |= adc1_config_channel_atten(ADC1_CHANNEL_3, lna_config->adc_atten);
    result |= adc1_config_width(lna_config->adc_bits_width);

    // CLEAR: SAR ADC1 controlled by RTC ADC1 CTRL
    CLEAR_PERI_REG_MASK(SENS_SAR_READ_CTRL_REG, SENS_SAR1_DIG_FORCE);

    // SAR Clock Divider - TBC
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_CLK_DIV, SAR1_CLK_DIV, SENS_SAR1_CLK_DIV_S);

    if (result != ESP_OK ) {
        ESP_LOGE(TAG, "Failure initializing LNA");
    }
    return result;
}


uint32_t adc1_lna_get_value(uint16_t stage1_cycles, uint16_t stage3_cycles)
{
    // SET: SAR ADC1 controller (in RTC) is started by SW
    SET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_FORCE);
    // XPD_SAR controlled by FSM
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 0, SENS_FORCE_XPD_SAR_S);
    // XPD_AMP controlled by FSM
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_AMP, 0, SENS_FORCE_XPD_AMP_S);

    // enable ADC1_CH0 (SENSOR_VP) - TBC
    // open the ADC1 data port, but no pads are enabled
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD, 0, SENS_SAR1_EN_PAD_S);
    // SET: SAR ADC1 pad enable bitmap is controlled by SW
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 1, SENS_SAR1_EN_PAD_FORCE_S);

    /* Configure AMP FSM
     *
     * FSM controls sampling, charge transfer, and conversion process.
     *
     * It has 4 stages, see "Structure of Low Noise Amplifier" figure
     * in ESP32 Technical Reference Manual:
     *
     * - stage 1: charge external sampling capacitors
     * - stage 2: intermediate stage, the sampling capacitors are disconnected from
     *            both the input and internal capacitors
     * - stage 3: charge transfer from external sampling capacitors to the internal capacitors
     * - stage 4: ADC conversion
     *
     * What happens in hardware at each of the stages is configured using the
     * fields SENS_SAR_MEAS_CTRL_REG. For each field:
     * - BIT(3) - means that corresponding block is enabled at stage 1
     * - BIT(2) - at stage 2
     * - BIT(1) - at stage 3
     * - BIT(0) - at stage 4
     */
    const int STAGE1 = BIT(3);
    const int STAGE2 = BIT(2);
    const int STAGE3 = BIT(1);
    const int STAGE4 = BIT(0);
    const int OFF = 0;
    const int ALL = STAGE1 | STAGE2 | STAGE3 | STAGE4;

    // SHORT_REF_GND switch is always open
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_AMP_SHORT_REF_GND_FSM, OFF );
    // SHORT_REF switch is closed at charge transfer and ADC conversion stages
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_AMP_SHORT_REF_FSM, STAGE3 | STAGE4);
    // SAR ADC is enabled at charge transfer and ADC conversion stages
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_SAR_RSTB_FSM, STAGE3 | STAGE4);
    // RST_FB switch is closed at initial sampling stage
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_AMP_RST_FB_FSM, STAGE1);
    // AMP and the ADC are enabled at all stages
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_XPD_SAR_AMP_FSM, ALL);
    REG_SET_FIELD(SENS_SAR_MEAS_CTRL_REG, SENS_XPD_SAR_FSM, ALL);

    // Set durations of each stage, in RTC_FAST_CLK cycles
    const int stage2_cycles = 10;
    REG_SET_FIELD(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT1, stage1_cycles);
    REG_SET_FIELD(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT2, stage2_cycles);
    REG_SET_FIELD(SENS_SAR_MEAS_WAIT2_REG, SENS_SAR_AMP_WAIT3, stage3_cycles);

    // wait det_fsm == 0
    while(GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR1_REG, 0x7, SENS_MEAS_STATUS_S) != 0)
    {
        ets_delay_us(1);
    }

    // Trigger force start
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 0, SENS_MEAS1_START_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 1, SENS_MEAS1_START_SAR_S);
    // Wait for done flag to be set
    while (GET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DONE_SAR) == 0)
    {
        ets_delay_us(1);
    }

    return GET_PERI_REG_BITS2(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DATA_SAR, SENS_MEAS1_DATA_SAR_S);
}


float lna_adc_to_mv(adc_mv_cal_t cal, adc_bits_width_t ibw, int adc)
{
    int sb = 3 - ibw;
    return cal.mv_b + (cal.mv_t - cal.mv_b)
            * (adc - (cal.adc_b>>sb)) / ((cal.adc_t>>sb) - (cal.adc_b>>sb));
}

