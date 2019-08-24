/* ESP32 LNA Thermocouple Example (https://github.com/krzychb/esp32-lna)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <ESP32LNA.h>
#include <SimpleDHT.h>

#define ADC_WIDTH_BIT              (10)
#define ADC_ATTEN             (ADC_0db)

#define ADC_SAMPLE_COUNT          (256)
#define ADC_TABLE_WIDTH            (15)
#define LNA_STAGE_1_CYCLES       (1024)
#define LNA_STAGE_3_CYCLES         (64)

#define COLD_JUCTION_TEMPERATURE   (25)

#define AMS2302_PIN                 (5)


/*  
  LNA Calibration
   
  The purpose of calibration is establish relationship between signal value
  at LNA input and resulting ADC readings. A liner relationship is assumed.

  Procedure:

  Use a thermocouple or other mV signal source to drive LNA input

    1. First apply a signal in a negative mV range to obtain bottom ADC reading
    2. Enter obtained ADC reading and mV values in table below
    4. Then apply a signal in a positive mV range to obtain top ADC reading
    5. Enter obtained ADC reading and mV values in table below

    Note: Select signals is a range that do not cause ADC readings
          to get steady to 0 or to saturate at the maximum

*/
const adc_mv_cal_t thk_cal = {
    -1.000,  // bottom mV input signal value  
      1694,  // bottom ADC reading
     2.599,  // top mV input signal value
      3680   // top ADC reading
};


// data source: http://cn.omega.com/learning/ITS-90T-CPoly.html
#define POLY_N (6)
const double poly_k[] = {
    0,          // 0
    2.508e-2,   // 1
    7.860e-8,   // 2
   -2.503e-10,  // 3
    8.315e-14,  // 4
   -1.228e-17   // 5
};


ESP32LNA lna;
SimpleDHT22 dht22(AMS2302_PIN);


double calc_temperature(float milivolts)
{
    double temperature = poly_k[0];

    for (int i=1; i<POLY_N; i++) {
        temperature += poly_k[i] * pow(1000*milivolts, i);
    }
    return temperature;
}


void setup()
{

  Serial.begin(115200);
  delay(1000);
  
  Serial.print("Initializing LNA...");
  Serial.println( lna.init(ADC_WIDTH_BIT, ADC_ATTEN) ? "done" : "failed");
}


int16_t get_cold_junction_temperature(void)
{
    int result = SimpleDHTErrSuccess;
    byte temperature = COLD_JUCTION_TEMPERATURE;
    byte humidity = 0;
    char logString[60];

    if ((result = dht22.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
        sprintf(logString, "Read AMS2302 failed (%d)\n", result);
        Serial.print(logString);
    }
    return temperature;
}


void loop()
{
  char logString[60];
      
  while (1) {
    int adc_sum = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        adc_sum += lna.getValue(LNA_STAGE_1_CYCLES, LNA_STAGE_3_CYCLES);
    }
  
    adc_sum /= ADC_SAMPLE_COUNT;
    double milivolts = lna.adc_to_mv(thk_cal, ADC_WIDTH_BIT, adc_sum);
    double temperature = calc_temperature(milivolts);
    int16_t temperature_cj = get_cold_junction_temperature();
    temperature += temperature_cj;
    sprintf(logString, "Thermocouple: %.3f mV, %.0f oC, Cold junction temperature: %d oC, ADC raw count: %d\n",
            milivolts, temperature, temperature_cj, adc_sum);
    Serial.print(logString);
    delay(1000);
  }
}
