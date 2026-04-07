#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

#include <test.hpp>

static const char* TAG = "main";

void nuke(WSG_MEM& w) { w.nuke(); }

WSG_MEM test_init()
{
    WSG_MEM wsg_mem;
    // wsg_mem.nuke();
    return wsg_mem;
}

void test_meta()
{
    WSG_MEM wsg_mem;
    std::array<uint8_t, 9> r;
    wsg_mem.read_meta(r);
}

void test_write_column(WSG_MEM& w)
{
    w.nuke();
    std::array<uint16_t, 3> wsgs = {1, 2, 3};
    w.write(wsgs, 1, 1);
    std::vector<uint8_t> rx_data;
    w.read_page(1);
}

void stress_test(WSG_MEM& w)
{
    std::array<uint16_t, 3> x = {1, 2, 3};
    for (int i = 2; i < 10; i++) {
        w.write(x, i, 1);
    }

    for (int i = 2; i < 10; i++) {
        w.read_page(i);
    }
}

void test_indiv_overflow(WSG_MEM& w)
{
    ESP_LOGI(TAG, "Testing overflow?");
    std::vector<uint32_t> wsgs = {1, 2, 3};
    w.indiv_write(0, wsgs);
}

WSG_MEM tune_min_delay(void)
{
    WSG_MEM wsg_mem = test_init();

    return wsg_mem;
}
void hi() { return; }
extern "C" void app_main(void)
{
    w25n04kv_init_param_t flash_params = {
        .cs_pin = GPIO_NUM_41,
        .wp_pin = GPIO_NUM_NC,
        .spi_host = SPI2_HOST
    };
    
    WSG_MEM wsg_mem = test_init();
    wsg_mem.init(flash_params);
    wsg_mem.nuke();

    wsg_mem.init_meta(FIRST_PAGE, 0, 1, 0);
    // test_indiv_overflow(wsg_mem);
}