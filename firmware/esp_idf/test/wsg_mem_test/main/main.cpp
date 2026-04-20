#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

#include <test.hpp>

static const char* TAG = "FLASH MEM TEST";

void nuke(WSG_MEM& w) { w.nuke(); }

WSG_MEM wsg_mem;

void test_meta()
{
    std::vector<uint8_t> r(METADATA_SIZE);
    esp_err_t err = wsg_mem.read_meta(r);
    assert(err == ESP_OK);

    assert(wsg_mem.meta_empty(r));

    err = wsg_mem.init_meta(FIRST_PAGE, 0, 1, 32);
    assert(err == ESP_OK);
    err = wsg_mem.read_meta(r);
    assert(err == ESP_OK && !wsg_mem.meta_empty(r));
    
    wsg_mem.interpret_meta_data(r);
    assert(wsg_mem.get_last_page() == 1);
    assert(wsg_mem.get_last_column() == 0);
    assert(wsg_mem.wsg_id == 1);
    assert(wsg_mem.dac_bias == 32);
}

// TODO: expand test to test multiples pages;
void test_write_data() {
    std::vector<uint16_t> strain_data = {1, 2, 3};

    wsg_mem.nuke();

    esp_err_t err;
    int items_written = 0;
    
    for(int i = 0; i < PAGE_SIZE / sizeof(wsg_data)) {
        err = wsg_mem.indiv_write(i, strain_data);

        items_written++;
        
        // increment wsg data
        for(int j = 0; j < 3; j++)
            strain_data[j]++;

        ASSERT(err == ESP_OK);
    }

    ESP_LOGI("test_write_data", "Wrote %d items to flash", items_written);

    for (int k = 0)

    std::vector<wsg_data> wsgs = wsg_mem.read_wsg_page(1);
    for(int i = 0; i < wsgs.length(); i++) {
        // check that timestamp is correct
        ASSERT(wsgs[i].time == i);

        // check that data is correct
    }

}
// void test_write_column(WSG_MEM& w)
// {
//     w.nuke();
//     std::array<uint16_t, 3> wsgs = {1, 2, 3};
//     w.write(wsgs, 1, 1);
//     std::vector<uint8_t> rx_data;
//     w.read_page(1);
// }

// void stress_test(WSG_MEM& w)
// {
//     std::array<uint16_t, 3> x = {1, 2, 3};
//     for (int i = 2; i < 10; i++) {
//         w.write(x, i, 1);
//     }

//     for (int i = 2; i < 10; i++) {
//         w.read_page(i);
//     }
// }

// void test_indiv_overflow(WSG_MEM& w)
// {
//     ESP_LOGI(TAG, "Testing overflow?");
//     std::vector<uint32_t> wsgs = {1, 2, 3};
//     w.indiv_write(0, wsgs);
// }

// WSG_MEM tune_min_delay(void)
// {
//     WSG_MEM wsg_mem = test_init();

//     return wsg_mem;
// }
// void hi() { return; }
extern "C" void app_main(void)
{
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));
    spi_cfg.mosi_io_num   = GPIO_NUM_9;
    spi_cfg.miso_io_num   = GPIO_NUM_8;
    spi_cfg.sclk_io_num   = GPIO_NUM_7;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", err);
        return;
    }

    w25n04kv_init_param_t flash_params = {
        .cs_pin = GPIO_NUM_41,
        .wp_pin = GPIO_NUM_NC,
        .spi_host = SPI2_HOST
    };
    
    wsg_mem.init(flash_params);
    wsg_mem.nuke();
    test_meta();
    test_write_data();
}