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
    std::vector<uint8_t> r;
    wsg_mem.read_meta(r);
}

void test_write_column(WSG_MEM& w)
{
    w.nuke();
    std::vector<uint32_t> wsgs = {1, 2, 3};
    w.write(wsgs, 1, 1);
    std::vector<uint8_t> rx_data;
    w.read_page(1);
}

void stress_test(WSG_MEM& w)
{
    std::vector<uint32_t> x = {1, 2, 3};
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
    w.indiv_write(wsgs);
}

WSG_MEM tune_min_delay(void)
{
    WSG_MEM wsg_mem = test_init();

    return wsg_mem;
}
void hi() { return; }
extern "C" void app_main(void)
{
    WSG_MEM wsg_mem = test_init();
    wsg_mem.nuke();

    // test_indiv_overflow(wsg_mem);
}