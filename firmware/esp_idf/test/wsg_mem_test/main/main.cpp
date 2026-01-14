#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

#include <test.hpp>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    WSG_MEM wsg_mem;
    wsg_mem.read_and_interpret_meta();
}