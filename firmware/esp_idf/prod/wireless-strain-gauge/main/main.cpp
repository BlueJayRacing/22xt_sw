#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <driveSensorSetup.hpp>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <ntp_udp_client.hpp>
#include <stdio.h>
#include <udp_client.hpp>
#include <vector>
#include <wsg_mem.hpp>

// #define DATA_READ_ONLY true
// #define FLASH_MEM true
#define USE_UDP true

static const char* TAG                           = "main";
static const drive_cfg::channel_t SG_CHANNELS[3] = {drive_cfg_t::STRAIN_GAUGE_0, drive_cfg_t::STRAIN_GAUGE_1,
                                                    drive_cfg_t::STRAIN_GAUGE_2};

QueueHandle_t flash_mem_q;
TaskHandle_t write_handle;
TaskHandle_t data_read_handle;
UBaseType_t sem_val = 1;
UdpClient client;
WSG_MEM wsg_mem;
int dac_bias = 0;
uint8_t wsg_id;

const ads1120_init_param_t ads1120_params = {
    .cs_pin = GPIO_NUM_38, 
    .drdy_pin = GPIO_NUM_8, 
    .spi_host = SPI2_HOST
};

ad5626_init_param_t ad5626_params {
    .cs_pin = GPIO_NUM_37,
    .ldac_pin = GPIO_NUM_36,
    .clr_pin = GPIO_NUM_NC,
    .spi_host = SPI2_HOST
};

void vTaskFlashWrite(void* pvParameter);
esp_err_t serialize_msg_and_publish(std::array<wsg_data_t, 6> data_arr);
void vTaskDataProcessing(void* pvParameter);
void vTaskCalibrate(void * pvParameter);

extern "C" void app_main(void) { 
    ESP_LOGI(TAG, "Starting up wsg main");
    // stall till udp client startup
    SocketHandler socket_handle;

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

#ifndef DATA_READ_ONLY
#ifdef FLASH_MEM
    w25n04kv_init_param_t flash_params = {
        .cs_pin = GPIO_NUM_41,
        .wp_pin = GPIO_NUM_NC,
        .spi_host = SPI2_HOST
    };

    wsg_mem.init(flash_params);

    // start data queue for flash
    flash_mem_q = xQueueCreate(10, sizeof(wsg_data_t*));
#endif

    // connect to wifi
    err = client.initialize_wifi_connection();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to connect to main board over wifi");
    }

    err = client.initialize_socket();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to initialize udp socket");
    }

    // ESP_LOGI(TAG, "Waiting for go signal");
    // Message * msg = nullptr;
    // std::array<uint8_t, MESSAGE_MAX_LEN> tx_data = {0};
    // // later change this to actual wsg id
    // tx_data[0] = {0x01};

    // while (msg == nullptr) {
    //     // error check here
    //     err = client.publish_data(0, tx_data, 1);
    //     msg = client.recv_data();
    // }
    // xTaskCreatePinnedToCore(vTaskDataProcessing, "data processing thread", (1 << 16), NULL, 3, &data_read_handle, (UBaseType_t)1);
    start_client_timesync_loop();
    taskYIELD();
    ESP_LOGI(TAG, "Finished sync");
    // if (msg->payload_len == 1 && msg->payload[0] == 0x08) {
    //     free(msg);

#ifdef FLASH_MEM
        // start tasks
        xTaskCreatePinnedToCore(vTaskFlashWrite, "flash memory write thread", (1 << 16), NULL, 3, &write_handle, (UBaseType_t)0);
#endif
#endif
        ESP_LOGI(TAG, "STARTED SYNC");
        xTaskCreatePinnedToCore(vTaskDataProcessing, "data processing thread", (1 << 16), NULL, 3, &data_read_handle, (UBaseType_t)1);
// #ifndef DATA_READ_ONLY
//     } else if (msg->payload_len <= 2 && msg->payload[0] == 0x04) {
//         // start calibration task
//         if (msg->payload_len == 2) {
//             wsg_mem.set_wsg_id(msg->payload[1]);
//             if (err != ESP_OK) {
//                 ESP_LOGE(TAG, "Error setting the id (%d) in flash: %s", msg->payload[1], esp_err_to_name(err));
//             }
//         }

//         free(msg);

//         xTaskCreate(vTaskCalibrate, "calibration thread", (1<<16), NULL, 1, NULL);
//     } else {
//         ESP_LOGE(TAG, "Failed to boot due to incorrect spi instruction: %d, payload len: %d", msg->payload[0], msg->payload_len);
//     }
// #endif

    vTaskDelete(NULL);
}

void vTaskCalibrate(void * pvParameter) {
    driveSensorSetup sensors;
    sensors.init(ads1120_params, ad5626_params);
    uint16_t new_dac_bias = 0;

    esp_err_t err = sensors.zero(&new_dac_bias);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error zeroing sensors: %s", esp_err_to_name(err));
        vTaskDelete( NULL );
    }

#ifdef FLASH_MEM
    err = wsg_mem.set_dac_bias(new_dac_bias);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting the dac bias in flash: %s", esp_err_to_name(err));
    }
#endif

    xTaskCreate(vTaskDataProcessing, "calibration data sending", (1<<16), NULL, 2, NULL);
    vTaskDelete( NULL );
}

// task for reading data/publishing udp
void vTaskDataProcessing(void* pvParameter)
{
    ESP_LOGI(TAG, "Starting data processing task");
    driveSensorSetup sensors;
    esp_err_t err = sensors.init(ads1120_params, ad5626_params);

#ifdef FLASH_MEM 
    sensors.setDACValue(wsg_mem.dac_bias);
#else
    sensors.setDACValue(0);
    dac_bias = 0x15;
#endif

    int array_ct = 0;
    std::array<wsg_data_t, 6> udp_data_buf;
    wsg_data_t* sample;
    drive_measurement_t measure;

    drive_cfg_t drive_cfg;
    drive_cfg.mode = drive_cfg_t::MEASURING_MODE;

    ESP_LOGI(TAG, "INIT SHIT");

    while (1) {
        sample = new wsg_data_t();
        memset(sample, 0, sizeof(wsg_data_t));
        sample->wsg_id = 1;
        sample->timestamp = get_timestamp();
        sample->dac_bias  = dac_bias;

        // ESP_LOGI(TAG, "SAMPLING %llu", sample->timestamp);

        for (int i = 0; i < 3; i++) {
            drive_cfg.channel = SG_CHANNELS[i];
            sensors.configure(drive_cfg);
            vTaskDelay(5);
            sensors.measure(true, &measure);

            sample->sample[i] = measure.adc_value;
        }

#ifndef DATA_READ_ONLY
#ifdef FLASH_MEM
        if (xQueueSend(flash_mem_q, sample, 0) != pdPASS) {
            delete sample;
            ESP_LOGW(TAG, "failed to add sample to flash mem queue");
        }

        if (!uxQueueSpacesAvailable(flash_mem_q)) {
            // Notify the writer to do the writing
            xTaskNotifyGiveIndexed(write_handle, sem_val);
        }
#endif

        // Pass data to the mainboard
        udp_data_buf[array_ct] = *sample;
        array_ct++;

        if (array_ct == 6) {
            serialize_msg_and_publish(udp_data_buf);
            array_ct = 0;
        }

#endif
        delete sample;
        // vTaskDelay(pdMS_TO_TICKS(1000));
        // taskYIELD();
        // esp_task_wdt_reset();
    }
}

// should we use a handler?
// task to write for flash
void vTaskFlashWrite(void* pvParameter)
{
    while (1) {
        uint32_t notif_val = ulTaskNotifyTakeIndexed(sem_val, pdTRUE, portMAX_DELAY);
        if (notif_val != 1) {
            continue;
        }

        wsg_data_t* sample = nullptr;

        while (xQueueReceive(flash_mem_q, sample, 10) == pdPASS) {
            // put stuff in write format
            if (sample != nullptr) {
                std::vector<uint16_t> buf(sample->sample.begin(), sample->sample.end());
                wsg_mem.indiv_write(sample->timestamp, buf);
            }
        }

        xTaskNotifyGiveIndexed(data_read_handle, sem_val);
        // esp_task_wdt_reset();
        taskYIELD();
    }
}

esp_err_t serialize_msg_and_publish(std::array<wsg_data_t, 6> data_arr)
{
    std::array<uint8_t, MESSAGE_MAX_LEN> send_data = {0};

    std::array<uint8_t, 19> temp_d;
    for (int i = 0; i < data_arr.size(); i++) {
        temp_d = serialize_wsg_data(data_arr[i]);
        // ESP_LOGI(TAG, "%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x", temp_d[0], temp_d[1], temp_d[2], temp_d[3], temp_d[4], temp_d[5], temp_d[6], temp_d[7], temp_d[8], temp_d[9], temp_d[10], temp_d[11], temp_d[12], temp_d[13], temp_d[14], temp_d[15], temp_d[16], temp_d[17], temp_d[18]);
        std::copy(temp_d.begin(), temp_d.end(), send_data.begin() + (i * 19));
    }

    esp_err_t err = client.publish_data(get_timestamp(), send_data, send_data.size());

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to serialize and publish data");
    }

    return err;
}