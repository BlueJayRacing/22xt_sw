#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib> // rand, srand
#include <cstring> // memset

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/time.h> // gettimeofday
#include <vector>
#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <fstream>
#include <iostream>

#include "esp_check.h"
#include "esp_event.h"
#include <esp_system.h>
#include "esp_http_client.h"
#include "esp_log.h"

#include "w25n04kv.hpp"
#include "wsg_mem.hpp"
static const char* TAG = "main";

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
    return ret;
}

std::vector<wsg_data> WSG_MEM::interpret_read_data(std::vector<uint8_t>& rx_data)
{
    int len = rx_data.size() / CHUNK_SIZE;
    std::vector<wsg_data> wsgs(len);
    for (int l = 0; l < len; l++) {
        int base = l * CHUNK_SIZE;
        wsg_data w;
        w.time = 0;
        for (int i = 0; i < 8; i++) {
            w.time |= ((uint64_t)rx_data[base + i] << ((7 - i) * 8));
        }
        uint32_t wsg = 0;
        int ij       = base + 8;
        for (int i = 0; i < 3; i++) {
            wsg = 0;
            for (int j = 0; j < 4; j++) {
                ij = base + 8 + (4 * i) + j;
                wsg |= ((uint32_t)rx_data[ij] << ((3 - j) * 8));
            }
            w.wsgs[i] = wsg;
        }
        wsgs[l] = w;
    }

    return wsgs;
}

std::vector<uint8_t> WSG_MEM::format_send_data(std::vector<uint32_t>& wsgs)
{
    //
    // means each is 8 bytes + 3 * 4 = 20 bytes or 128 bits
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    uint64_t time;

    time = (uint64_t)cur_time.tv_sec * 1000000L + (uint64_t)cur_time.tv_usec;
    ESP_LOGI(TAG, "Current time: %llu", time);
    std::vector<uint8_t> output(CHUNK_SIZE);

    // splitting time into 8 bytes
    for (int i = 0; i < 8; i++) {
        output[i]     = (time >> ((7 - i) * 8)) & 0xFF;
        std::string s = std::format("{:x}", output[i]);
    }
    // splitting wsg data
    for (int i = 0; i < 3; i++) {
        uint32_t wsg = wsgs[i];
        int ij;
        for (int j = 0; j < 4; j++) {
            ij         = 8 + (4 * i) + j;
            output[ij] = (wsg >> ((3 - j) * 8)) & 0xFF;
        }
    }

    return output;
}

esp_err_t WSG_MEM::write(std::vector<uint32_t>& wsgs, uint32_t page_addr, uint16_t column_addr)
{

    std::vector<uint8_t> tx_data = format_send_data(wsgs);

    ESP_LOGI(TAG, "Writing page");

    for (int i = 0; i < CHUNK_SIZE; i++) {
        std::string s = std::format("{:x}", tx_data[i]);
        ESP_LOGI(TAG, "Data in hex %s", s.c_str());
        ESP_LOGI(TAG, "Write data %d", tx_data[i]);
    }

    esp_err_t ret = spi_flash_.writePage(tx_data, page_addr);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write. :(. Page number: %lu, Column number %u, error %d", page_addr, column_addr, ret);
        return ret;
    }

    return ESP_OK;
}

esp_err_t WSG_MEM::read_all(uint32_t page_addr, uint16_t column_addr)
{

    // read first page and figure out what last page/column is _
    ESP_LOGI(TAG, "working?? read_all? hello? page addr %u, column %u", page_addr, column_addr);
    esp_err_t ret;
    ret = read_and_interpret_meta(page_addr, column_addr);
    wait_for_ready();
    std::vector<uint8_t> rx_data(W25N04KV::PAGE_SIZE);
    // std::ofstream Data("wsgdata.csv");
    // Data << "Time, WSG1, WSG2, WSG3 \n";
    for (uint32_t i = 1; i < page_addr + 1; i++) {
        ret = spi_flash_.readPage(rx_data, i, column_addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed. :(). Page number %lu, error %d", i, ret);
            return ret;
        }
        ESP_LOGI(TAG, "Current read page number: %lu", i);
        for (int j = 0; j < CHUNK_SIZE; j++) {
            ESP_LOGI(TAG, "Read data %d", rx_data[j]);
        }
        std::vector<uint8_t> chunk(rx_data.begin(), rx_data.begin() + CHUNK_SIZE);
        std::vector<wsg_data> wsgs = interpret_read_data(chunk);
        for (wsg_data w : wsgs) {
            ESP_LOGI(TAG, "Time: %llu", w.time);
            for (int j = 0; j < 3; j++) {
                ESP_LOGI(TAG, "WSG Values: %lu", (unsigned long)w.wsgs[j]);
            }
            // Data << std::to_string(w.time) << "," << std::to_string(w.wsgs[0]) << "," << std::to_string(w.wsgs[1])
            //      << "," << std::to_string(w.wsgs[2]) << "\n";
        }
        wait_for_ready();
    }
    return ret;
}

void WSG_MEM::interpret_meta_data(std::vector<uint8_t>& rx_data, uint32_t& page_addr, uint16_t& column_addr)
{
    page_addr   = 0;
    column_addr = 0;

    for (int i = 0; i < 4; i++) {
        page_addr |= (uint32_t)rx_data[i] << ((3 - i) * 8);
        // std::string s = std::format("{:x}", output[i]);
    }
    for (int i = 4; i < 6; i++) {
        column_addr |= ((uint32_t)rx_data[i] << ((5 - i) * 8));
        // std::string s = std::format("{:x}", output[i]);
    }
}

esp_err_t WSG_MEM::update_meta(uint32_t page_addr, uint16_t column_addr)
{
    // 4+2 = 6
    std::vector<uint8_t> tx_data(METADATA_SIZE);

    // updating first page with metadata
    for (int i = 0; i < 4; i++) {
        tx_data[i] = (page_addr >> ((3 - i) * 8)) & 0xFF;
        // std::string s = std::format("{:x}", output[i]);
    }
    for (int i = 0; i < 2; i++) {
        tx_data[i + 4] = (column_addr >> ((1 - i) * 8)) & 0xFF;
        // std::string s = std::format("{:x}", output[i]);
    }

    esp_err_t ret = spi_flash_.writePage(tx_data, 0, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AHHHHH. Failed writing metadata. Error %d", ret);
    }
    return ret;
}

esp_err_t WSG_MEM::read_meta(std::vector<uint8_t>& rx_data)
{
    esp_err_t ret = spi_flash_.readPage(rx_data, 0);

    return ret;
}

esp_err_t WSG_MEM::read_and_interpret_meta(uint32_t& page_addr, uint16_t& column_addr)
{
    esp_err_t ret;
    // update_meta(page_addr, column_addr);
    // wait_for_ready();

    std::vector<uint8_t> metadata(METADATA_SIZE);
    ret = read_meta(metadata);

    for (int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Read metadata %d", metadata[i]);
    }

    interpret_meta_data(metadata, page_addr, column_addr);
    ESP_LOGI(TAG, "Metadata: Page address: %u, Column address: %u", page_addr, column_addr);
    return ret;
}

esp_err_t WSG_MEM::reset(uint32_t last_page, uint16_t last_column)
{
    esp_err_t ret;

    uint32_t page_addr;
    uint16_t column_addr;
    read_and_interpret_meta(page_addr, column_addr);

    int last_block = page_addr / block_size;

    for (int i = 0; i < last_block + 1; i++) {
        spi_flash_.enableWrite();
        wait_for_ready();
        ret = spi_flash_.eraseBlock(i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase block: %d. Block number: %d", ret, i);
        }
        wait_for_ready();
    }
    wait_for_ready();
    ret = update_meta(last_page, last_column);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update metadata.");
    }
}
esp_err_t WSG_MEM::indiv_write(std::vector<uint32_t>& wsgs, uint32_t& page_addr, uint16_t& column_addr, int i)
{
    if ((i % METADATA_UPDATE_INT) == 0 && i != 0) {
        update_meta(page_addr, column_addr);
        wait_for_ready();
    }

    if (((column_addr + CHUNK_SIZE) >= 2047)) {
        page_addr++;
        column_addr = 0;
        ESP_LOGI(TAG, "Incremented Page: %0x", page_addr);
    }
    if (page_addr >= W25N04KV::NUM_PAGES) {
        ESP_LOGE(TAG, "Page out of bounds :(. Addr: %0x", page_addr);
        return;
    } else {
        ret = write(wsgs, i, column_addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data: %d", ret);
            return ret;
        }
    }
    column_addr += (CHUNK_SIZE);
    return ret;
}
esp_err_t WSG_MEM::cont_write(std::vector<uint32_t>& wsg_data)
{

    // cont_write assumes t
    esp_err_t ret;
    init();
    uint32_t page_addr;
    uint16_t column_addr;

    read_and_interpret_meta(page_addr, column_addr);
    for (int i = 0; i < 16; i++) // condition is while data is being read? not sure how to do yet
    {
        wait_for_ready();
        vTaskDelay(2);
        ret = indiv_write(wsg_data, page_addr, column_addr, i);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ret;
}
