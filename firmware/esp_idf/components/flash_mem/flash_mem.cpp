#include "flash_mem.hpp"
#include <stdio.h>


WSG_MEM::WSG_MEM() {}

esp_err_t WSG_MEM::wait_for_ready(int timeout)
{

    int i = 0;
    w25n04kv_device_status_t status;

    while (i < timeout) {
        spi_flash_.readStatus(&status);
        if (status.is_busy == 0) {
            vTaskDelay(10);
            return ESP_OK;
        }
        vTaskDelay(10);
        i += 10;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t WSG_MEM::init()
{
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));

    spi_cfg.mosi_io_num   = SPI2_MOSI_PIN;
    spi_cfg.miso_io_num   = SPI2_MISO_PIN;
    spi_cfg.sclk_io_num   = SPI2_SCLK_PIN;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

    w25n04kv_init_param_t flash;

    flash.cs_pin   = GPIO_NUM_1;
    flash.wp_pin   = GPIO_NUM_NC;
    flash.spi_host = SPI2_HOST;
    esp_err_t ret;
    ret = spi_flash_.init(flash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI Flash: %d", ret);
    }
    spi_flash_.reset();

    spi_flash_.isCorrectDevice();
    spi_flash_.enableWrite();
    wait_for_ready();
    spi_flash_.printStatusReg();
    spi_flash_.printConfigReg();

}