#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

static const char* TAG = "FLASH MEM READ";

#define START_PAGE 64

extern "C" void app_main(void)
{
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));

    spi_cfg.mosi_io_num   = GPIO_NUM_9;//18;//9;
    spi_cfg.miso_io_num   = GPIO_NUM_8;//20;//8;
    spi_cfg.sclk_io_num   = GPIO_NUM_7;//19;//7;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

    w25n04kv_init_param_t flash_init_params;
    flash_init_params.cs_pin   = GPIO_NUM_34;//1;//26;
    flash_init_params.wp_pin   = GPIO_NUM_NC;
    flash_init_params.spi_host = SPI2_HOST;


    WSG_MEM wsg_mem;

    vTaskDelay(100);

    wsg_mem.init(flash_init_params);

    vTaskDelay(100);

    ESP_LOGI(TAG, "WSG ID: %d", wsg_mem.wsg_id);
    ESP_LOGI(TAG, "DAC BIAS: %u", wsg_mem.dac_bias);
    ESP_LOGI(TAG, "LAST PAGE: %u", wsg_mem.get_last_page());
    ESP_LOGI(TAG, "LAST COL: %u", wsg_mem.get_last_column());

    for (uint32_t page = START_PAGE; page <= wsg_mem.get_last_page(); page++) {
        vTaskDelay(100);
        wsg_mem.wait_for_ready();
        std::vector<wsg_data> wsgs = wsg_mem.read_wsg_page(page);

        for (int i = 0; i < PAGE_SIZE / CHUNK_SIZE; i++)
            ESP_LOGI(TAG, "Read page %u ts: %llu and data values %u, %u, %u", page, wsgs[i].time, wsgs[i].wsgs[0], wsgs[i].wsgs[1], wsgs[i].wsgs[2]);
    }
}