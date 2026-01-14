#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#includ
#include <udp_client.hpp>
#include <driveSensorSetup.hpp>

static const char* TAG = "main";

typedef struct var_pkt {

} var_pkt_t;

QueueHandle_t flash_mem_q;
TaskHandle_t write_handle;
UBaseType_t sem_val = 1;

extern "C" void app_main(void)
{

}

void startup() {
    // stall till udp client startup
    SocketHandler socket_handle;

    // sensor init
    ads1120_init_param_t ads1120_params = {
        .cs_pin = 38;
        .drdy_pin = 0
        .spi_host
    }

    // stall till
    // read wsg num from flash
    // read dac biases from flash

    // start data queue for flash
    flash_mem_q = xQueueCreate(10, sizeof(wsg_data_t *));

    // start tasks
    vTaskCreate(vTaskFlashWrite, "flash memory write thread", (1<<8), NULL, 2, &write_handle);
    vTaskCreate(vTaskDataProcessing, "data processing thread", (1<<8), NULL, 1, NULL)
}

// task for reading data/publishing udp
void vTaskDataProcessing(void * pvParameter) {


    // if idx == 10
    // give

    if(!uxQueueSpacesAvailable(flash_mem_q)) {
        xTaskNotifyGiveIndexed(write_handle, sem_val);
    }

    // other stuff

}



// should we use a handler?
// task to write for flash
void vTaskFlashWrite(void * pvParameter) {
    while (1) {
        uint32_t notif_val = xTaskNotifyTakeIndexed(sem_val, pdTRUE, portMAX_DELAY)
        if(notif_val != 1) {
            continue;
        }
    

        // write

        // give
    }
}
