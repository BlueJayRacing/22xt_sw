#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <udp_client.hpp>
#include <driveSensorSetup.hpp>
#include <algorithm>


#define SPI_MOSI_PIN    18
#define SPI_SCLK_PIN    30

static const char* TAG = "main";

typedef struct var_pkt {

} var_pkt_t;

QueueHandle_t flash_mem_q;
TaskHandle_t write_handle;
UBaseType_t sem_val = 1;
UdpClient client;
int dac_bias = -1;

extern "C" void app_main(void)
{
    startupDrive();
}

void startupDrive() {
    // stall till udp client startup
    SocketHandler socket_handle;

    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));
    spi_cfg.mosi_io_num = GPIO_NUM_31;
    spi_cfg.miso_io_num = GPIO_NUM_32;
    spi_cfg.sclk_io_num = GPIO_NUM_30;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", err);
        return;
    }

    // stall till

    err = client.initialize_wifi_connection();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to connect to main board over wifi");
    }

    err = client.initialize_socket();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to initialize udp socket");
    }

    // read wsg num from flash
    // read dac biases from flash

    // start data queue for flash
    flash_mem_q = xQueueCreate(10, sizeof(wsg_data_t *));

    // should we wait for startup message from main board

    // the other codes either activates calibration from pi or it activates drive, we can assume drive but it would be interesting to also think about cal

    // start tasks
    vTaskCreate(vTaskFlashWrite, "flash memory write thread", (1<<8), NULL, 2, &write_handle);
    vTaskCreate(vTaskDataProcessing, "data processing thread", (1<<8), NULL, 1, NULL)
}

// task for reading data/publishing udp
void vTaskDataProcessing(void * pvParameter) {

        // sensor init
    ads1120_init_param_t ads1120_params = {
        .cs_pin = GPIO_NUM_38,
        .drdy_pin = GPIO_NUM_NC,
        .spi_host = SPI2_HOST
    };

    ad5626_init_param_t ad5626_params = {
        .cs_pin = GPIO_NUM_37,
        .ldac_pin = GPIO_NUM_NC,
        .clr_pin = GPIO_NUM_NC,
        .spi_host = SPI2_HOST
    };

    driveSensorSetup sensors;
    sensors.init(ads1120_params, ad5626_params);

    sensors.setDacBias(dac_bias)

    uint8_t array_ct = 0;
    std::array<wsg_data_t, 5> udp_data_buf;

    wsg_data_t * sample;

    while (1) {
        sample = new wsg_data_t();
        memset(sample, 0, sizeof(wsg_data_t));

        drive_measurement_t measure;

        // THIS MEASUREMENT DOESN'T WORK LOL
        sensor.measure(true, &measure);

        sample->timestamp = get_timestamp();
        sample->dac_bias = dac_bias;

        // store data in sample

        std

        if(xQueueSend(flash_mem_q, sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "failed to add sample to queue");
        }

        if(!uxQueueSpacesAvailable(flash_mem_q)) {
            xTaskNotifyGiveIndexed(write_handle, sem_val);
        }

        // other stuff
        udp_data_buf[array_ct++] = &sample;
        
        if (array_ct == 5) {
            serialize_msg_and_publish(udp_data_buf);
        }
    }

}



// should we use a handler?
// task to write for flash
void vTaskFlashWrite(void * pvParameter) {
    while (1) {
        uint32_t notif_val = xTaskNotifyTakeIndexed(sem_val, pdTRUE, portMAX_DELAY)
        if(notif_val != 1) {
            continue;
        }
    
        wsg_data_t * sample;
        while (xQueueReceive(flash_mem_q, sample, 10) == pdPASS) {
            // put stuff in write format

            delete sample;
        }

        // write data to flash in one go or not
    }
}


esp_err_t serialize_msg_and_publish(std::array<wsg_data_t, 5> data_arr) {
    std::array<uint8_t, 125> send_data;

    std::array<uint8_t 25> temp_d;
    for (int i = 0; i < data_arr.size(); i++) {
        temp_d = serialize_wsg_data(data_arr[i]);
        std::copy(temp_d.begin(), temp_d.end(), result.begin() + (i * 25));
    }

    esp_err_t err = client.publish_data(get_timestamp(), temp_d, temp_d.size());
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to serialize and publish data");
    }

    return err;

}