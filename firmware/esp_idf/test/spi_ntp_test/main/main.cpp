#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <spi_ntp.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    NTPviaSPI spi_sync = NTPviaSPI(SPI1_HOST);
    esp_err_t err = spi_sync.sync();
    if(err == ESP_OK) {
        ESP_LOGI(TAG, "Sync completed succesfully");
    } else {
        ESP_LOGE(TAG, "Failed to sync: %d", err);
    }
}