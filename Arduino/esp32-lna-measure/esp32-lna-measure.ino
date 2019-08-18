/* ESP32 LNA Measure Example (https://github.com/krzychb/esp32-lna)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <ESP32LNA.h>
#include <SimpleDHT.h>

#define ADC_WIDTH_BIT        (9)
#define ADC_ATTEN      (ADC_0db)

#define ADC_SAMPLE_COUNT   (256)
#define ADC_TABLE_WIDTH     (15)

#define AMS2302_PIN          (5)

ESP32LNA lna;
SimpleDHT22 dht22(AMS2302_PIN);


void setup() {

  Serial.begin(115200);
  delay(1000);
  
  Serial.print("Initializing LNA...");
  Serial.println( lna.init(ADC_WIDTH_BIT, ADC_ATTEN) ? "done" : "failed");
  
  Serial.print("Table below contains LNA readings depending on stage 1 and 3 cycles.\n");
  Serial.print("Stage 1 cycles are in first line, stage 3 cycles - in first column of the table.\n\n");
}


void print_temperature_humidity(void)
{
    int result = SimpleDHTErrSuccess;
    byte temperature = 0;
    byte humidity = 0;
    char logString[60];

    if ((result = dht22.read(&temperature, &humidity, NULL)) == SimpleDHTErrSuccess) {
        sprintf(logString, "Ambient temperature: %d oC, humidity: %d %%\n", temperature, humidity);
        Serial.print(logString);
    } else {
        sprintf(logString, "Read AMS2302 failed (%d)\n", result);
        Serial.print(logString);
    }
}


void lna_stage_profile(void)
{
    uint32_t adc_sum;
    uint16_t lna_stage1_cycles = 1;
    uint16_t lna_stage3_cycles = 1;
    char logString[60];

    print_temperature_humidity(); // comment out this line
                                  // if temperature sensor is not connected
    sprintf(logString, "ADC: %d bit, Attenuation: (%d)\n", ADC_WIDTH_BIT, ADC_ATTEN);
    Serial.print(logString);

    Serial.print("   0; ");
    for (int ist1 = 1; ist1 < ADC_TABLE_WIDTH; ist1++) {
        sprintf(logString, "%4d; ", lna_stage1_cycles);
        Serial.print(logString);
        lna_stage1_cycles *= 2;
    }
    Serial.print("\n");
    for (int ist3 = 1; ist3 < ADC_TABLE_WIDTH; ist3++) {
        lna_stage1_cycles = 1;
        sprintf(logString, "%4d; ", lna_stage3_cycles);
        Serial.print(logString);
        for (int ist1 = 1; ist1 < ADC_TABLE_WIDTH; ist1++) {
            adc_sum = 0;
            for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
                adc_sum += lna.getValue(lna_stage1_cycles, lna_stage3_cycles);
            }
            sprintf(logString, "%4d; ", adc_sum / ADC_SAMPLE_COUNT);
            Serial.print(logString);
            lna_stage1_cycles *= 2;
            // delay so watch dog does not kick in
            delay(10);
        }
        Serial.print("\n");
        lna_stage3_cycles *= 2;
    }
    Serial.print("\n");
}


void loop()
{
  lna_stage_profile();
  delay(1000);
}
