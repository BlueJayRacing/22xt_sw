#pragma once

#include <inttypes.h>
#include "esp_check.h"
#include "esp_event.h"

#define SPI2_MOSI_PIN       18
#define SPI2_MISO_PIN       20
#define SPI2_SCLK_PIN       19
#define METADATA_UPDATE_INT 5
#define CHUNK_SIZE    20
#define METADATA_SIZE 6

struct wsg_data {
    uint64_t time;
    std::array<uint32_t, 3> wsgs;
};

class WSG_MEM {
  public:
    WSG_MEM();
    esp_err_t wait_for_ready(int timeout = 1000);
    esp_err_t init();
    esp_err_t write(std::vector<uint32_t>& wsgs, uint32_t page_addr, uint16_t column_addr);
    esp_err_t read_all(uint32_t page_addr, uint16_t column_addr);
    esp_err_t update_meta(uint32_t page_addr, uint16_t column_addr);
    esp_err_t reset(uint32_t last_page, uint16_t last_column = 0);
    esp_err_t read_and_interpret_meta(uint32_t& page_addr, uint16_t& column_addr);
    esp_err_t indiv_write(std::vector<uint32_t>& wsgs, uint32_t& page_addr, uint16_t& column_addr, int i);
    esp_err_t cont_write(std::vector<uint32_t>& wsg_data);


  private:
    uint8_t block_size = (1 << 6);
    std::vector<wsg_data> interpret_read_data(std::vector<uint8_t>& rx_data);
    std::vector<uint8_t> format_send_data(std::vector<uint32_t>& wsgs);
    void interpret_meta_data(std::vector<uint8_t>& rx_data, uint32_t& page_addr, uint16_t& column_addr);
    esp_err_t read_meta(std::vector<uint8_t>& rx_data);
    W25N04KV spi_flash_;


};