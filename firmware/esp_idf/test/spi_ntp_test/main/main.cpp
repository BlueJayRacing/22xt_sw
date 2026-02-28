#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <spi_ntp.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    NTPviaSPI *spi_sync = new NTPviaSPI(SPI2_HOST);
    esp_err_t err = spi_sync->sync();
    if(err == ESP_OK) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Sync completed succesfully");
    } else {
        ESP_LOGI(TAG, "Failed to sync: %d", err);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
