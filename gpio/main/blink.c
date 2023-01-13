#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

uint8_t counter = 0;
void isr_handler(void* arg){
    counter++;
}

gpio_config_t gpio_i={
    .pin_bit_mask= (1 << GPIO_NUM_2),
    .mode= GPIO_MODE_OUTPUT,
}; 

gpio_config_t gpio_o={
    .pin_bit_mask= (1 << GPIO_NUM_26),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en= GPIO_PULLUP_ENABLE,
    .intr_type= GPIO_INTR_NEGEDGE
};

void app_main(void)
{   
    gpio_config(&gpio_i);
    gpio_config(&gpio_o);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(GPIO_NUM_26, isr_handler, (void*) GPIO_NUM_26);
    while (1){
        gpio_set_level(GPIO_NUM_2, 1);
        printf("%d \n", counter);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
