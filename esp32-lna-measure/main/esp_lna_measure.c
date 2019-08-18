/* ESP32 LNA Measure Example (https://github.com/krzychb/esp32-lna)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lna.h"
#include "dht.h"


#define ADC_WIDTH_BIT      (ADC_WIDTH_BIT_9)
#define ADC_ATTEN            (ADC_ATTEN_0db)

#define ADC_SAMPLE_COUNT               (256)
#define ADC_TABLE_WIDTH                 (15)

lna_config_t lna_config = {
    .adc_bits_width = ADC_WIDTH_BIT,
    .adc_atten = ADC_ATTEN
};


void print_temperature_humidity(void)
{
    dht_sensor_type_t sensor_type = DHT_TYPE_AM2301;
    gpio_num_t dht_gpio = 5;
    int16_t temperature = 0;
    int16_t humidity = 0;

    esp_err_t result = dht_read_data(sensor_type, dht_gpio, &humidity, &temperature);
    if (result == ESP_OK) {
        printf("Ambient temperature: %d oC, humidity: %d %%\n", temperature / 10, humidity / 10);
    } else {
        printf("Could not read data from temperature sensor\n");
    }
}


void lna_stage_profile(void)
{
    uint32_t adc_sum;
    uint16_t lna_stage1_cycles = 1;
    uint16_t lna_stage3_cycles = 1;

    print_temperature_humidity(); // comment out this line
                                  // if temperature sensor is not connected
    printf("ADC: %d bit, Attenuation: (%d)\n", ADC_WIDTH_BIT+9, ADC_ATTEN);

    printf("%5d;", 0);
    for (int ist1 = 1; ist1 < ADC_TABLE_WIDTH; ist1++) {
        printf("%5d;", lna_stage1_cycles);
        lna_stage1_cycles *= 2;
    }
    printf("\n");
    adc1_lna_init(&lna_config);
    for (int ist3 = 1; ist3 < ADC_TABLE_WIDTH; ist3++) {
        lna_stage1_cycles = 1;
        printf("%5d;", lna_stage3_cycles);
        for (int ist1 = 1; ist1 < ADC_TABLE_WIDTH; ist1++) {
            adc_sum = 0;
            for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
                adc_sum += adc1_lna_get_value(lna_stage1_cycles, lna_stage3_cycles);
            }
            printf("%5d;", adc_sum / ADC_SAMPLE_COUNT);
            lna_stage1_cycles *= 2;
            // delay so watch dog does not kick in
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        printf("\n");
        lna_stage3_cycles *= 2;
    }
    printf("\n");
}


void app_main(void)
{
    printf("Table below contains LNA readings depending on stage 1 and 3 cycles.\n");
    printf("Stage 1 cycles are in first line, stage 3 cycles - in first column of the table.\n\n");
    while (1) {
        lna_stage_profile();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
