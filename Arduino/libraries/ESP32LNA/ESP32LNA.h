/**
 *  @filename   :   ESP32LNA.h
 *  @brief      :   Header file of ESP32LNA.cpp providing interface functions
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

#ifndef ESP32LNA_H
#define ESP32LNA_H

#include <Arduino.h>


class ESP32LNA
{
    public:
        ESP32LNA();

        bool init(uint8_t adc_bits_width, adc_attenuation_t adc_attenuation);
        uint16_t getValue(uint16_t stage1_cycles, uint16_t stage3_cycles); 
};

#endif  /* ESP32LNA_H */
