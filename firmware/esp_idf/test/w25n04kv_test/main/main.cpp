#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <test.hpp>
#include <cstring>

#include <w25n04kv.hpp>

#define SPI2_MOSI_PIN 18
#define SPI2_MISO_PIN 20
#define SPI2_SCLK_PIN 19

W25N04KV spi_flash_;

static const char* TAG = "main";

extern "C" void app_main(void)
{
    // Test test(ESP_LOG_DEBUG);

    // test.testW25N04KV();

    esp_err_t ret;
    
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));

    spi_cfg.mosi_io_num   = SPI2_MOSI_PIN;
    spi_cfg.miso_io_num   = SPI2_MISO_PIN;
    spi_cfg.sclk_io_num   = SPI2_SCLK_PIN;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

    w25n04kv_init_param_t flash_init_params;
    flash_init_params.cs_pin = GPIO_NUM_1;
    flash_init_params.wp_pin = GPIO_NUM_NC;
    flash_init_params.spi_host = SPI2_HOST;

    ESP_LOGI(TAG, "Initialized SPI Bus");

    vTaskDelay(100);

    ret = spi_flash_.init(flash_init_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI Flash: %d", ret);
        return;
    }

    std::vector<uint8_t> tx_data(100);
    std::vector<uint8_t> rx_data(100);

    std::srand(esp_cpu_get_cycle_count());

    for (int i = 0; i < 100; i++) {
        tx_data.at(i) = rand() % 255;
    }

    spi_flash_.reset();

    spi_flash_.isCorrectDevice();
    spi_flash_.enableWrite();

    vTaskDelay(2);

    spi_flash_.printStatusReg();
    spi_flash_.printConfigReg();

    uint32_t page_address = 0b00000010000000000; //std::rand() % W25N04KV::NUM_PAGES;

    // 0xxxxx00

    ESP_LOGI(TAG, "Page address: %d", (int) page_address);

    ESP_LOGI(TAG, "Erasing page");

    // spi_flash_.eraseBlock(page_address);// & 0x1FFC0);

    vTaskDelay(1);

    ESP_LOGI(TAG, "Writing page");

    for (int i = 0; i < 100; i++) {
        ESP_LOGI(TAG, "Write data %d", tx_data[i]);
        ESP_LOGI(TAG, "RX data %d", rx_data[i]);
    }

    spi_flash_.writePage(tx_data, page_address, 0);

    spi_flash_.printStatusReg();

    int k = 1;
    for (int i = 0; i < k; i++) {
        vTaskDelay(5);
        spi_flash_.readPage(rx_data, page_address);
        for (int i = 0; i < 100; i++) {
            ESP_LOGI(TAG, "Read data %d", rx_data[i]);
        }
    }

    ESP_LOGI(TAG, "Initialized SPI Flash");
}