#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>

#include <ad5626.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{


    ESP_LOGI(TAG, "STARTING TEST");
    AD5626 dac_;

    ad5626_init_param_t dac_params;
    dac_params.cs_pin   = GPIO_NUM_37;
    dac_params.ldac_pin = GPIO_NUM_36;
    dac_params.clr_pin  = GPIO_NUM_NC;
    dac_params.spi_host = SPI2_HOST;

    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));

    spi_cfg.mosi_io_num   = GPIO_NUM_9;
    spi_cfg.miso_io_num   = GPIO_NUM_8;
    spi_cfg.sclk_io_num   = GPIO_NUM_7;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", ret);
        return;
    }

    ret = dac_.init(dac_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AD5626: %d", ret);
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Testing setting different values on DAC");

        int dac_value = 0;
        int dac_incr  = AD5626::MAX_LEVEL_VALUE / 10;

        for (int i = 0; i <= 10; i++) {
            dac_.setLevel(dac_value);
            ESP_LOGI(TAG, "Level: %d, Voltage %f", dac_value, 3.3f * dac_value / AD5626::MAX_LEVEL_VALUE);
            vTaskDelay(pdMS_TO_TICKS(1000));
            dac_value += dac_incr;
        }

        ESP_LOGI(TAG, "Passed setting values on DAC");
    }
}