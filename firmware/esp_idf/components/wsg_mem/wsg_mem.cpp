#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib> // rand, srand
#include <cstring> // memset

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/time.h> // gettimeofday
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include <fstream>
#include <iostream>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <esp_system.h>

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

bool WSG_MEM::is_initialized(std::vector<uint8_t> meta)
{
    return !std::all_of(meta.cbegin(), meta.cend(), [](int i) { return i == 1; });
}
esp_err_t WSG_MEM::init_meta(uint32_t dac_bias_, uint8_t wsg_id_)
{
    update_meta(1, 0); // initial page = 1, initial column = 0

    std::vector<uint8_t> tx_data(METADATA_SIZE - PAGE_COLUMN_META_SIZE);

    // set ID and DAC bias
    tx_data[PAGE_COLUMN_META_SIZE] = wsg_id_;
    for (int i = PAGE_COLUMN_META_SIZE + 1; i < METADATA_SIZE; i++) {
        tx_data[i] = (dac_bias_ >> ((3 - (i - (PAGE_COLUMN_META_SIZE + 1))) * 8)) & 0xFF;
        // splits into individual bits. subtract the page_column +1 because that was the initial value.
    } // multiply by 8 bc its byte and mask to change it into one byte
    esp_err_t ret = spi_flash_.writePage(tx_data, 0, PAGE_COLUMN_META_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AHHHHH. Failed writing metadata. Error %d", ret);
    }
    return ret;
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

    // meta data initialization
    std::vector<uint8_t> metadata(W25N04KV::PAGE_SIZE);
    ret = read_meta(metadata);
    if (!is_initialized(metadata)) {
        init_meta(WSG_ID, DAC_BIAS);
    }
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

esp_err_t WSG_MEM::read_all(uint32_t page_addr, uint16_t column_addr, std::vector<uint8_t>& rx_data)
{

    // read first page and figure out what last page/column is _
    ESP_LOGI(TAG, "working?? read_all? hello? page addr %u, column %u", last_page, last_column);
    esp_err_t ret;
    ret = read_and_interpret_meta();
    wait_for_ready();

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
        }
        wait_for_ready();
    }
    return ret;
}

void WSG_MEM::interpret_meta_data(std::vector<uint8_t>& rx_data)
{
    last_page   = 0;
    last_column = 0;

    for (int i = 0; i < 4; i++) {
        last_page |= (uint32_t)rx_data[i] << ((3 - i) * 8);
        // std::string s = std::format("{:x}", output[i]);
    }
    for (int i = 4; i < 6; i++) {
        last_column |= ((uint16_t)rx_data[i] << ((5 - i) * 8));
        // std::string s = std::format("{:x}", output[i]);
    }
    rx_data[PAGE_COLUMN_META_SIZE] = wsg_id;

    for (int i = PAGE_COLUMN_META_SIZE + 1; i < METADATA_SIZE; i++) {
        dac_bias |= ((uint32_t)rx_data[i] << ((METADATA_SIZE - 1 - i) * 8));
    }
}
esp_err_t WSG_MEM::update_meta(uint32_t page_addr, uint16_t column_addr)
{
    // 4+2 = 6
    std::vector<uint8_t> tx_data(PAGE_COLUMN_META_SIZE);

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

esp_err_t WSG_MEM::read_and_interpret_meta()
{
    esp_err_t ret;
    // update_meta(page_addr, column_addr);
    // wait_for_ready();

    std::vector<uint8_t> metadata(PAGE_COLUMN_META_SIZE);
    // here add logic about if the meta is not initialized, initialize it. or maybe thats in init.
    ret = read_meta(metadata);
    wait_for_ready();
    for (int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Read metadata %d", metadata[i]);
    }

    interpret_meta_data(metadata);
    ESP_LOGI(TAG, "Metadata: Page address: %u, Column address: %u", last_page, last_column);
    return ret;
}

esp_err_t WSG_MEM::reset()
{
    esp_err_t ret;

    read_and_interpret_meta();

    int last_block = last_page / block_size;

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
    return ret;
}

esp_err_t WSG_MEM::indiv_write(std::vector<uint32_t>& wsgs, int i)
{
    esp_err_t ret;
    if ((i % METADATA_UPDATE_INT) == 0 && i != 0) {
        update_meta(last_page, last_column);
        wait_for_ready();
    }

    if (((last_column + CHUNK_SIZE) >= 2047)) {
        last_page++;
        last_column = 0;
        ESP_LOGI(TAG, "Incremented Page: %0x", last_page);
    }
    if (last_page >= W25N04KV::NUM_PAGES) {
        ESP_LOGE(TAG, "Page out of bounds :(. Addr: %0x", last_page);
        return ESP_ERR_INVALID_SIZE;
    } else {
        ret = write(wsgs, i, last_column);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data: %d", ret);
            return ret;
        }
    }
    last_column += (CHUNK_SIZE);
    return ESP_OK;
}

esp_err_t WSG_MEM::cont_write(std::vector<uint32_t>& wsg_data)
{

    // cont_write assumes t
    esp_err_t ret;
    init();

    read_and_interpret_meta();
    for (int i = 0; i < 16; i++) // condition is while data is being read? not sure how to do yet
    {
        wait_for_ready();
        vTaskDelay(2);
        ret = indiv_write(wsg_data, i);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ret;
}
// esp_err_t WSG_MEM::read_and_send() { std::vector<uint8_t> rx_data(W25N04KV::PAGE_SIZE); }
