#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <wsg_mem.hpp>

// CHANGE THIS PER WSG
// *******************************
#define WSG_ID 0
#define WSG_INITIAL_DAC_BIAS 0
#define WSG_INITIAL_PAGE FIRST_PAGE
#define WSG_INITIAL_COL 0
#define WIPE_DATA true
// *******************************

static const char* TAG = "Wsg meta write";

extern "C" void app_main(void)
{
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));

    spi_cfg.mosi_io_num   = GPIO_NUM_9;//18;//9;
    spi_cfg.miso_io_num   = GPIO_NUM_8;//20;//8;
    spi_cfg.sclk_io_num   = GPIO_NUM_7;//19;//7;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);

    w25n04kv_init_param_t flash_init_params;
    flash_init_params.cs_pin   = GPIO_NUM_34;//1;
    flash_init_params.wp_pin   = GPIO_NUM_NC;
    flash_init_params.spi_host = SPI2_HOST;


    WSG_MEM wsg_mem;

    vTaskDelay(100);

    wsg_mem.init(flash_init_params);

    if (WIPE_DATA) {
        wsg_mem.nuke();
        vTaskDelay(100);
        wsg_mem.erase_all_flash();
    }
    
    wsg_mem.init_meta(WSG_INITIAL_PAGE, WSG_INITIAL_COL, WSG_ID, WSG_INITIAL_DAC_BIAS);
}