#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

#include <test.hpp>

static const char* TAG = "FLASH MEM TEST";

void nuke(WSG_MEM& w) { w.nuke(); }

void test_meta(WSG_MEM& wsg_mem)
{
    wsg_mem.nuke();
    std::vector<uint8_t> r(METADATA_SIZE);
    esp_err_t err = wsg_mem.read_meta(r);
    assert(err == ESP_OK);

    for ( int i = 0; i < METADATA_SIZE; i++) {
        ESP_LOGI(TAG, "%d, %u", i, r[i]);
    }

    assert(wsg_mem.meta_empty(r));

    wsg_mem.wait_for_ready();
    err = wsg_mem.init_meta(FIRST_PAGE, 0, 1, 32);
    assert(err == ESP_OK);
    // err = wsg_mem.read_meta(r);
    // assert(err == ESP_OK && !wsg_mem.meta_empty(r));

    // wsg_mem.interpret_meta_data(r);
    wsg_mem.read_and_interpret_meta();
    assert(wsg_mem.get_last_page() == FIRST_PAGE);
    assert(wsg_mem.get_last_column() == 0);
    assert(wsg_mem.wsg_id == 1);
    assert(wsg_mem.dac_bias == 32);

    ESP_LOGI(TAG, "PASSED METADATA TEST");
}

// TODO: expand test to test multiples pages;
void test_write_data_page(WSG_MEM& wsg_mem)
{
    std::vector<uint16_t> strain_data = {1, 1<<8, 1<<13};

    // wsg_mem.nuke();

    esp_err_t err;
    int items_written = 0;

    wsg_mem.flash_erase_block(wsg_mem.get_last_page());

    // PAGE_SIZE / CHUNK_SIZE
    for (int i = 0; i < PAGE_SIZE / CHUNK_SIZE; i++) {
        // wsg_mem.wait_for_ready();
        err = wsg_mem.indiv_write(i, strain_data);

        items_written++;

        // increment wsg data
        for (int j = 0; j < 3; j++)
            strain_data[j]++;

        assert(err == ESP_OK);
    }

    wsg_mem.flash_write_exec();

    // ESP_LOGI("test_write_data", "Wrote %d items to flash at page: %d", items_written, wsg_mem.get_last_page());

    strain_data[0] = 1;
    strain_data[1] = 1<<8;
    strain_data[2] = 1<<13;

    vTaskDelay(100);
    wsg_mem.wait_for_ready();
    std::vector<wsg_data> wsgs = wsg_mem.read_wsg_page(wsg_mem.get_last_page());
    // ESP_LOGI("test_write_data", "num read from page: %d", wsgs.size());
    // assert(wsgs.size() == items_written);

    for (int i = 0; i < items_written; i++) {
        // check that timestamp is correct
        // ESP_LOGI("test_write_data", "Read %d ts: %llu and data values %u, %u, %u", i, wsgs[i].time, wsgs[i].wsgs[0],
                // wsgs[i].wsgs[1], wsgs[i].wsgs[2]);
        assert(wsgs[i].time == i);

        // // check that data is correct
        // ESP_LOGI(TAG, "SD0: %u, %u", strain_data[0], wsgs[i].wsgs[0]);
        assert(strain_data[0] == wsgs[i].wsgs[0]);
        assert(strain_data[1] == wsgs[i].wsgs[1]);
        assert(strain_data[2] == wsgs[i].wsgs[2]);

        for (int j = 0; j < 3; j++)
            strain_data[j]++;
    }

    ESP_LOGI(TAG, "PASSED PAGE WRITE TEST");
}

void test_multi_page_write(WSG_MEM& wsg_mem, int num_pages) {
    std::vector<uint16_t> strain_data = {1, 1<<8, 1<<14};

    // wsg_mem.nuke();

    esp_err_t err;
    int items_written = 0;

    uint32_t start_page = wsg_mem.get_last_page();
    uint16_t start_col = wsg_mem.get_last_column();

    // PAGE_SIZE / CHUNK_SIZE
    for (uint64_t i = 0; i < (PAGE_SIZE / CHUNK_SIZE) * num_pages; i++) {
        // wsg_mem.wait_for_ready();
        err = wsg_mem.indiv_write(i<<35, strain_data);

        items_written++;

        // increment wsg data
        for (int j = 0; j < 3; j++)
            strain_data[j]++;

        assert(err == ESP_OK);
    }

    wsg_mem.flash_write_exec();

    ESP_LOGI("test multi page write", "Wrote %u items to flash, started at %u, %u and ended at %u, %u page", items_written, start_page, start_col, wsg_mem.get_last_page(), wsg_mem.get_last_column());

    // assert(wsg_mem.get_last_page() == start_page + 10);

    strain_data[0] = 1;
    strain_data[1] = 1<<8;
    strain_data[2] = 1<<14;

    for (uint32_t page = start_page; page <= wsg_mem.get_last_page(); page++) {
        vTaskDelay(100);
        wsg_mem.wait_for_ready();
        std::vector<wsg_data> wsgs = wsg_mem.read_wsg_page(page);
        // ESP_LOGI("test_write_data", "num read from page: %d", wsgs.size());
        // assert(wsgs.size() == items_written);

        for (uint64_t i = 0; i < PAGE_SIZE / CHUNK_SIZE; i++) {
            // check that timestamp is correct
            ESP_LOGI("test_write_data", "Read %llu ts: %llu and data values %u, %u, %u", i, wsgs[i].time, wsgs[i].wsgs[0],
                    wsgs[i].wsgs[1], wsgs[i].wsgs[2]);
            assert(wsgs[i].time == (i + (page - start_page) * (PAGE_SIZE / CHUNK_SIZE))<<35);

            // // check that data is correct
            // ESP_LOGI(TAG, "SD0: %u, %u", strain_data[0], wsgs[i].wsgs[0]);
            assert(strain_data[0] == wsgs[i].wsgs[0]);
            assert(strain_data[1] == wsgs[i].wsgs[1]);
            assert(strain_data[2] == wsgs[i].wsgs[2]);

            for (int j = 0; j < 3; j++)
                strain_data[j]++;
        }

        ESP_LOGI("test multi page write", "Passed for page %u", page);
    }

    ESP_LOGI(TAG, "passed multi page write");
}

// assumes read is working
void test_write_overflow(WSG_MEM& wsg_mem)
{
    std::vector<uint16_t> wsgs = {1, 2, 3};

    wsg_mem.set_last_col(2090);
    assert(wsg_mem.get_last_column() == 2090);

    uint32_t page = wsg_mem.get_last_page();

    wsg_mem.indiv_write(1, wsgs);

    assert(page + 1 == wsg_mem.get_last_page());

    assert(wsg_mem.page_empty(page, PAGE_SIZE));
    assert(!wsg_mem.page_empty(page + 1, PAGE_SIZE));
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

    spi_cfg.mosi_io_num   = GPIO_NUM_18;//9;
    spi_cfg.miso_io_num   = GPIO_NUM_20;//8;
    spi_cfg.sclk_io_num   = GPIO_NUM_19;//7;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

    w25n04kv_init_param_t flash_init_params;
    flash_init_params.cs_pin   = GPIO_NUM_1;//26;
    flash_init_params.wp_pin   = GPIO_NUM_NC;
    flash_init_params.spi_host = SPI2_HOST;


    WSG_MEM wsg_mem;

    vTaskDelay(100);

    wsg_mem.init(flash_init_params);

    wsg_mem.erase_all_flash();
    test_meta(wsg_mem);
    test_multi_page_write(wsg_mem, 5);
    test_write_data_page(wsg_mem);
}