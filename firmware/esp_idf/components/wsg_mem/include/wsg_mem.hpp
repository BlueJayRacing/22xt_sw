#pragma once
#include "esp_check.h"
#include "esp_event.h"
#include "w25n04kv.hpp"
#include <inttypes.h>
#include <array>

#define SPI2_MOSI_PIN         18
#define SPI2_MISO_PIN         20
#define SPI2_SCLK_PIN         19
#define METADATA_UPDATE_INT   5
#define CHUNK_SIZE            14 // 8 bytes for time + 6 bytes for wsg
#define PAGE_COLUMN_META_SIZE 6  // size for page and column metadata
#define METADATA_SIZE         9 // 6 for page/column + 1 for id + 2 for dac bias
#define META_PAGE             0
#define FIRST_PAGE            1
struct wsg_data {
    uint64_t time;
    std::array<uint16_t, 3> wsgs;
};

class WSG_MEM {
  public:
    WSG_MEM();

    esp_err_t wait_for_ready(int timeout = 1000);
    esp_err_t init(w25n04kv_init_param_t flash);
    esp_err_t write(uint64_t timestamp, std::vector<uint16_t>& wsgs, uint32_t page_addr, uint16_t column_addr);
    esp_err_t read_all(uint32_t page_addr, uint16_t column_addr, std::vector<uint8_t>& rx_data);
    esp_err_t update_meta(uint32_t page_addr, uint16_t column_addr);
    esp_err_t reset();
    esp_err_t read_and_interpret_meta();
    esp_err_t indiv_write(uint64_t timestamp, std::vector<uint16_t>& wsgs);
    esp_err_t cont_write(std::vector<uint16_t>& wsg_data);
    esp_err_t init_meta(uint32_t page, uint16_t column, uint8_t wsg_id_, uint16_t dac_bias_);
    esp_err_t read_meta(std::vector<uint8_t>& rx_data);
    void read_page(uint32_t page_addr);
    void nuke();
    esp_err_t set_dac_bias(uint16_t dac_bias);
    esp_err_t set_wsg_id(uint8_t id);
    void interpret_meta_data(std::vector<uint8_t>& rx_data);
    uint32_t get_last_page();
    uint16_t get_last_column();
    bool meta_empty(std::vector<uint8_t> meta);
    
    uint16_t dac_bias;
    uint8_t wsg_id;

  private:
    uint8_t block_size = (1 << 6);
    std::vector<wsg_data> interpret_read_data(std::vector<uint8_t>& rx_data);
    std::vector<uint8_t> format_send_data(uint64_t timestamp, std::vector<uint16_t>& wsgs);

    W25N04KV spi_flash_;
    uint32_t last_page   = 1;
    uint16_t last_column = 0;
    uint64_t data_count;
    bool meta_initialized;
};