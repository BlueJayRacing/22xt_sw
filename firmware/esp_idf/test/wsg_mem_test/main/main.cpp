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
    for (int i = 2; i < 5; i++) {
        w.write(x, i, 1);
    }

    for (int i = 2; i < 5; i++) {
        w.read_page(i);
    }
}

void tune_min_delay(void) { WSG_MEM wsg_mem = test_init(); }
extern "C" void app_main(void)
{
    WSG_MEM wsg_mem = test_init();
    stress_test(wsg_mem);
}