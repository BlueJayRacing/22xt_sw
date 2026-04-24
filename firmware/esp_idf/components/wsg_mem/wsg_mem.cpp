#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib> // rand, srand
#include <cstring> // memset

#include <driver/gpio.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/time.h> // gettimeofday

#include "freertos/FreeRTOS.h"

// #include <fstream>
// #include <iostream>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "include/wsg_mem.hpp"
#include <esp_system.h>

static const char* TAG = "wsg_mem";

WSG_MEM::WSG_MEM() {}

esp_err_t WSG_MEM::wait_for_ready(int timeout)
{
    int i = 0;
    w25n04kv_device_status_t status;

    while (i < timeout) {
        spi_flash_.readStatus(&status);
        if (status.is_busy) {
            ESP_LOGI(TAG, "Delayed by 10ms");
            vTaskDelay(10);
        } else {
            vTaskDelay(10);
            return ESP_OK;
        }
        i += 10;
    }
    return ESP_ERR_TIMEOUT;
}

bool WSG_MEM::meta_empty(std::vector<uint8_t> meta)
{
    for (int i = 0; i < METADATA_SIZE; i++) {
        if (meta[i] != 255) {
            return false;
        }
    }
    return true;
}

bool WSG_MEM::page_empty(uint32_t page, int page_size)
{
    std::vector<wsg_data> data = read_wsg_page(page);

    for (int i = 0; i < page_size; i++) {
        for(int j = 0; j < 3; j++)
        {
            if (data[i].wsgs[j] != 255) {
            return false;
            }
        }
        
    }
    return true;
}

esp_err_t WSG_MEM::set_dac_bias(uint16_t dac_bias) { return init_meta(last_page, last_column, wsg_id, dac_bias); }

esp_err_t WSG_MEM::set_wsg_id(uint8_t id) { return init_meta(last_page, last_column, id, dac_bias); }

void WSG_MEM::set_last_page(uint32_t page) { last_page = page; }

void WSG_MEM::set_last_col(uint16_t col) { last_column = col; }

esp_err_t WSG_MEM::init_meta(uint32_t page, uint16_t column, uint8_t wsg_id_, uint16_t dac_bias_)
{
    last_page   = page;
    last_column = column;
    wsg_id      = wsg_id_;
    dac_bias    = dac_bias_;

    std::vector<uint8_t> tx_data(METADATA_SIZE);
    // updating first page with metadata
    for (int i = 0; i < 4; i++) {
        tx_data[i] = (page >> ((3 - i) * 8)) & 0xFF;
        // std::string s = std::format("{:x}", output[i]);
    }
    for (int i = 0; i < 2; i++) {
        tx_data[i + 4] = (column >> ((1 - i) * 8)) & 0xFF;
        // std::string s = std::format("{:x}", output[i]);
    }

    // set ID and DAC bias
    ESP_LOGI(TAG, "id: %u", wsg_id_);
    tx_data[PAGE_COLUMN_META_SIZE] = wsg_id_;

    for (int i = 0; i < 2; i++) {
        tx_data[i + PAGE_COLUMN_META_SIZE + 1] = (dac_bias_ >> ((1 - i) * 8)) & 0xFF;
        // splits into individual bits. subtract the page_column +1 because that was the initial value.
    } // multiply by 8 bc its byte and mask to change it into one byte
    wait_for_ready();
    esp_err_t ret = spi_flash_.writePage(tx_data, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AHHHHH. Failed writing metadata. Error %d", ret);
    } else {
        ESP_LOGI(TAG, "Wrote metadata successfully!");
    }
    return ret;
}

esp_err_t WSG_MEM::init(w25n04kv_init_param_t flash)
{
    ESP_LOGI(TAG, "Initializing flash memory");

    esp_err_t ret;
    ret = spi_flash_.init(flash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI Flash: %d", ret);
    }
    wait_for_ready();
    spi_flash_.reset();
    wait_for_ready();
    spi_flash_.isCorrectDevice();
    spi_flash_.enableWrite();
    wait_for_ready();
    spi_flash_.printStatusReg();
    spi_flash_.printConfigReg();

    // wsg_id = wsg_id_;
    // dac_bias = dac_bias_;

    // meta data initialization
    ret = read_and_interpret_meta();

    data_count = 0;

    return ret;
}

std::vector<wsg_data> WSG_MEM::interpret_read_data(std::vector<uint8_t>& rx_data)
{
    int len = rx_data.size() / CHUNK_SIZE;
    std::vector<wsg_data> wsgs(len);
    // we need to read the timestamp and wsg values into the elements
    for (int l = 0; l < len; l++) {
        // the incoming data will be read in a large stream, and there are
        // l chunks of size chunk_size
        int base = l * CHUNK_SIZE;
        wsg_data w;
        // bit shift into the time
        w.time = 0;
        for (int i = 0; i < 8; i++) {
            w.time |= ((uint64_t)rx_data[base + i] << ((7 - i) * 8));
        }
        // iterate through the rest of the data, and bit shift into each wsg
        uint16_t wsg;
        int ij = base + 8;
        for (int i = 0; i < 3; i++) {
            wsg = 0;
            for (int j = 0; j < 2; j++) {
                ij = base + 8 + (2 * i) + j;
                wsg |= ((uint16_t)rx_data[ij] << ((1 - j) * 8));
            }
            w.wsgs[i] = wsg;
        }
        wsgs[l] = w;
    }

    return wsgs;
}

std::vector<uint8_t> WSG_MEM::format_send_data(uint64_t timestamp, std::vector<uint16_t>& wsgs)
{
    //
    // means each is 8 bytes + 3 * 4 = 20 bytes or 128 bits

    // ESP_LOGI(TAG, "Current time: %llu", timestamp);
    std::vector<uint8_t> output(CHUNK_SIZE);

    // splitting time into 8 bytes
    for (int i = 0; i < 8; i++) {
        output[i] = (timestamp >> ((7 - i) * 8)) & 0xFF;
        // std::string s = std::format("{:x}", output[i]);
    }

    ESP_LOGI(TAG, "ts data: %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x", output[0], output[1], output[2], output[3],
             output[4], output[5], output[6], output[7]);

    // splitting wsg data
    for (int i = 0; i < 3; i++) {
        uint16_t wsg = wsgs[i];
        int ij;
        for (int j = 0; j < 2; j++) {
            ij = 8 + (2 * i) + j;
            // initial value of 8 from time
            // +2 bytes for every wsg read
            // + j for every one of the two
            output[ij] = (wsg >> ((1 - j) * 8)) & 0xFF;
        }
    }

    return output;
}

esp_err_t WSG_MEM::write(uint64_t timestamp, std::vector<uint16_t>& wsgs, uint32_t page_addr, uint16_t column_addr)
{

    std::vector<uint8_t> tx_data = format_send_data(timestamp, wsgs);
    // std::array<uint8_t> tx_data = {1, 2, 3};
    // ESP_LOGI(TAG, "Writing page");

    for (int i = 0; i < CHUNK_SIZE; i++) { // instead of 3 should be chunk size
        // std::string s = std::format("{:x}", tx_data[i]);
        //  ESP_LOGI(TAG, "Data in hex %s", s.c_str());
        ESP_LOGI(TAG, "page: %lu, column: %lu, Write data %02x", page_addr, column_addr, tx_data[i]);
    }
    // vTaskDelay(50);
    esp_err_t ret = spi_flash_.writePage(tx_data, page_addr, column_addr);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write. :(. Page number: %lu, Column number %u, error %d", page_addr, column_addr, ret);
        return ret;
    }

    return ESP_OK;
}

std::vector<wsg_data> WSG_MEM::read_wsg_page(uint32_t page_addr)
{
    wait_for_ready();
    std::vector<uint8_t> rx_data(PAGE_SIZE);
    // ESP_LOGI(TAG, "read page %u", page_addr);
    spi_flash_.readPage(rx_data, page_addr, 0);
    for (int i = 0; i < 50; i++) {
        ESP_LOGW(TAG, "Byte %d: %02x", i, rx_data[i]);
    }
    // while(1) {};
    std::vector<wsg_data> wsgs = interpret_read_data(rx_data);
    return wsgs;
}

// TODO: consider only reading wsg data in chunks anyway

// esp_err_t WSG_MEM::read_all(uint32_t page_addr, uint16_t column_addr, std::array<uint8_t>& rx_data)
// {

//     // read first page and figure out what last page/column is _
//     ESP_LOGI(TAG, "working?? read_all? hello? page addr %u, column %u", last_page, last_column);
//     esp_err_t ret;
//     ret = read_and_interpret_meta();
//     vTaskDelay(80);

//     for (uint32_t i = FIRST_PAGE; i < page_addr + 1; i++) {
//         ret = spi_flash_.readPage(rx_data, i, column_addr);
//         if (ret != ESP_OK) {
//             ESP_LOGE(TAG, "failed. :(). Page number %lu, error %d", i, ret);
//             return ret;
//         }
//         ESP_LOGI(TAG, "Current read page number: %lu", i);
//         for (int j = 0; j < CHUNK_SIZE; j++) {
//             ESP_LOGI(TAG, "Read data %d", rx_data[j]);
//         }
//         std::array<uint8_t> chunk(rx_data.begin(), rx_data.begin() + CHUNK_SIZE);
//         std::vector<wsg_data> wsgs = interpret_read_data(chunk);
//         for (wsg_data w : wsgs) {
//             ESP_LOGI(TAG, "Time: %llu", w.time);
//             for (int j = 0; j < 3; j++) {
//                 ESP_LOGI(TAG, "WSG Values: %lu", (unsigned long)w.wsgs[j]);
//             }
//         }
//         vTaskDelay(80);
//     }
//     return ret;
// }

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
    wsg_id   = rx_data[PAGE_COLUMN_META_SIZE];
    dac_bias = 0;
    for (int i = PAGE_COLUMN_META_SIZE + 1; i < METADATA_SIZE; i++) {
        dac_bias |= ((uint32_t)rx_data[i] << ((METADATA_SIZE - 1 - i) * 8));
    }
}

uint32_t WSG_MEM::get_last_page() { return last_page; }

uint16_t WSG_MEM::get_last_column() { return last_column; }

esp_err_t WSG_MEM::update_meta(uint32_t page_addr, uint16_t column_addr)
{
    // erase block first, then rewrite metadata
    nuke();
    return init_meta(page_addr, column_addr, wsg_id, dac_bias);
}

esp_err_t WSG_MEM::read_meta(std::vector<uint8_t>& rx_data)
{
    // vTaskDelay(10);
    esp_err_t ret = spi_flash_.readPage(rx_data, META_PAGE, 0);
    // for (uint8_t i : rx_data) {
    //     ESP_LOGI(TAG, "Hi: %u", i);
    // }
    return ret;
}

esp_err_t WSG_MEM::read_and_interpret_meta()
{
    esp_err_t ret;
    // update_meta(page_addr, column_addr);
    // wait_for_ready();

    std::vector<uint8_t> metadata(METADATA_SIZE);
    ESP_LOGI(TAG, "Reading metadata");
    ret = read_meta(metadata);

    meta_initialized = !meta_empty(metadata);
    if (!meta_initialized) {
        ESP_LOGI(TAG, "Initializing meta");
        init_meta(FIRST_PAGE, 0, wsg_id, dac_bias);
        meta_initialized = true;
        read_meta(metadata);
    }

    for (int i = 0; i < METADATA_SIZE; i++) {
        ESP_LOGI(TAG, "Read metadata %d", metadata[i]);
    }

    interpret_meta_data(metadata);
    ESP_LOGI(TAG, "Metadata: Page address: %u, Column address: %u", last_page, last_column);
    ESP_LOGI(TAG, "WSG ID: %u", wsg_id);
    ESP_LOGI(TAG, "DAC Bias: %lu", dac_bias);

    return ret;
}

// esp_err_t WSG_MEM::reset()
// {
//     esp_err_t ret;
//     ESP_LOGI(TAG, "Reading and interpreting metadata to find last page: %lld", last_page);
//     read_and_interpret_meta();
//     int last_block = last_page / block_size;
//     wait_for_ready();

//     for (int i = 0; i < last_block + 1; i++) {
//         spi_flash_.enableWrite();
//         wait_for_ready();
//         ESP_LOGI(TAG, "Erasing block: %i", i);
//         ret = spi_flash_.eraseBlock(i);
//         if (ret != ESP_OK) {
//             ESP_LOGE(TAG, "Failed to erase block: %d. Block number: %d", ret, i);
//         }
//         wait_for_ready();
//     }
//     wait_for_ready();
//     ESP_LOGI(TAG, "Updating metadata with page 1, column 0 in second block");
//     ret = update_meta(FIRST_PAGE, 0);

//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to update metadata.");
//     }
//     return ret;
// }

void WSG_MEM::nuke()
{
    wait_for_ready();
    spi_flash_.enableWrite();
    wait_for_ready();
    ESP_LOGI(TAG, "Erasing first block");
    spi_flash_.eraseBlock(0);
    // wait_for_ready();
}
esp_err_t WSG_MEM::indiv_write(uint64_t timestamp, std::vector<uint16_t>& wsgs)
{
    esp_err_t ret;
    if ((last_page % METADATA_UPDATE_INT) == 0) {
        ESP_LOGI(TAG, "Updating metadata");
        update_meta(last_page, last_column);
        vTaskDelay(10);
    }
    ESP_LOGI(TAG, "Writing to page: %0x, column %0x", last_page, last_column);
    if (((last_column + CHUNK_SIZE) >= PAGE_SIZE)) {
        last_page++;
        last_column = 0;
        ESP_LOGI(TAG, "Incremented Page: %0x", last_page);
    }
    if (last_page >= W25N04KV::NUM_PAGES) {
        ESP_LOGE(TAG, "Page out of bounds :(. Addr: %0x", last_page);
        return ESP_ERR_INVALID_SIZE;
    } else {
        ret = write(timestamp, wsgs, last_page, last_column);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write data: %d", ret);
            return ret;
        }
    }
    last_column += (CHUNK_SIZE);
    return ESP_OK;
}

// esp_err_t WSG_MEM::indiv_write(uint64_t, std::array<uint8_t, 3> wsgs) {

// }

// esp_err_t WSG_MEM::cont_write(std::vector<uint16_t>& wsg_data)
// {

//     esp_err_t ret;
//     // init();

//     read_and_interpret_meta();
//     for (int i = 0; i < 16; i++) // condition is while data is being read? not sure how to do yet
//     {
//         wait_for_ready();
//         vTaskDelay(2);
//         ret = indiv_write(wsg_data);
//         if (ret != ESP_OK) {
//             return ret;
//         }
//     }
//     return ret;
// }
// esp_err_t WSG_MEM::read_and_send() { std::vector<uint8_t> rx_data(W25N04KV::PAGE_SIZE); }
