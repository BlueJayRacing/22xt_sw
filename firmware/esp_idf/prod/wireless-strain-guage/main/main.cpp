#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>

#include <udp_client.hpp>
#include <driveSensorSetup.hpp>

static const char* TAG = "main";

typedef struct var_pkt {

} var_pkt_t;

QueueHandle_t flash_mem_q;

extern "C" void app_main(void)
{

}

void startup() {
    // stall till udp client startup
    SocketHandler socket_handle;
    

    // stall till
    // read wsg num from flash
    // read dac biases from flash

    // start data queue for flash
    flash_mem_q = xQueueCreate(10, sizeof(wsg_data_t *));

    // start tasks
    vTaskCreate()
}

// task for reading data/publishing udp
void vTaskDataProcessing(void * pvParameter) {

    // if idx == 10
    // give

    // other stuff

}



// should we use a handler?
// task to write for flash
void vTaskDataProcessing(void * pvParameter) {
    while (1) {
        // wait for take

        // write

        // give
    }
}
