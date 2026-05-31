#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>

#include <test.hpp>
#include <ads1120.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "ALIVE");
    }
}