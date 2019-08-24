/**
 *  @filename   :   ESP32LNA.cpp
 *  @brief      :   Implementation of interface functions
 *                  of low noise amplifier on-board of ESP32
 *
 *  Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
 *  Copyright (C) Krzysztof Budzynski, August 17 2019
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "soc/sens_reg.h"
#include "esp32-hal-adc.h"

#include "ESP32LNA.h"

#define SENSOR_VP    (36) // CHANNEL_0 / GPIO36
#define SENSOR_CAPP  (37) // CHANNEL_1 / GPIO37
#define SENSOR_CAPN  (38) // CHANNEL_2 / GPIO38
#define SENSOR_VN    (39) // CHANNEL_3 / GPIO39

#define SAR1_CLK_DIV  (2)


ESP32LNA::ESP32LNA(void)
{
};


bool ESP32LNA::init(uint8_t adc_bits_width, adc_attenuation_t adc_attenuation)
{
    bool result;

    result  = adcAttachPin(SENSOR_VP);
    result |= adcAttachPin(SENSOR_CAPP);
    result |= adcAttachPin(SENSOR_CAPN);
    result |= adcAttachPin(SENSOR_VN);

    // Do we need to configure attenuation for all four channels?
    analogSetPinAttenuation(SENSOR_VP, adc_attenuation);
    analogSetPinAttenuation(SENSOR_VN, adc_attenuation);
    analogReadResolution(adc_bits_width);

    // CLEAR: SAR ADC1 controlled by RTC ADC1 CTRL
    CLEAR_PERI_REG_MASK(SENS_SAR_READ_CTRL_REG, SENS_SAR1_DIG_FORCE);

    // SAR Clock Divider - TBC
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_CLK_DIV, SAR1_CLK_DIV, SENS_SAR1_CLK_DIV_S);

    return(result);
};


uint16_t ESP32LNA::getValue(uint16_t stage1_cycles, uint16_t stage3_cycles)
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


float ESP32LNA::adc_to_mv(adc_mv_cal_t cal, uint8_t ibw, int adc)
{
    int sb = 12 - ibw;
    return cal.mv_b + (cal.mv_t - cal.mv_b)
            * (adc - (cal.adc_b>>sb)) / ((cal.adc_t>>sb) - (cal.adc_b>>sb));
}