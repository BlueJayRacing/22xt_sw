#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <driveSensorSetup.hpp>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <udp_client.hpp>
#include <ntp_udp_client.h>
#include <vector>
#include <wsg_mem.hpp>

#define SPI_MOSI_PIN 18
#define SPI_SCLK_PIN 30

static const char* TAG                           = "main";
static const drive_cfg::channel_t SG_CHANNELS[3] = {drive_cfg_t::STRAIN_GAUGE_0, drive_cfg_t::STRAIN_GAUGE_1,
                                                    drive_cfg_t::STRAIN_GAUGE_2};

QueueHandle_t flash_mem_q;
// QueueHandle_t pass_q;
TaskHandle_t write_handle;
// TaskHandle_t pass_handle;
UBaseType_t sem_val = 1;
UdpClient client;
WSG_MEM wsg_mem;
int dac_bias = -1;

void startupDrive(void);
void vTaskFlashWrite(void* pvParameter);
esp_err_t serialize_msg_and_publish(std::array<wsg_data_t, 6> data_arr);
void vTaskDataProcessing(void* pvParameter);

extern "C" void app_main(void) { startupDrive(); }

void startupDrive(void)
{
    // stall till udp client startup
    SocketHandler socket_handle;

    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));
    spi_cfg.mosi_io_num   = GPIO_NUM_31;
    spi_cfg.miso_io_num   = GPIO_NUM_32;
    spi_cfg.sclk_io_num   = GPIO_NUM_30;
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

    wsg_mem.read_and_interpret_meta();

    // start data queue for flash
    flash_mem_q = xQueueCreate(10, sizeof(wsg_data_t*));
    // pass_q = xQueueCreate(6, sizeof(wsg_data_t*));

    // should we wait for startup message from main board
    start_client_timesync_loop();

    // start tasks
    xTaskCreatePinnedToCore(vTaskFlashWrite, "flash memory write thread", (1 << 8), NULL, 2, &write_handle, (UBaseType_t)0);
    xTaskCreatePinnedToCore(vTaskDataProcessing, "data processing thread", (1 << 8), NULL, 1, NULL, (UBaseType_t)1);
    // xTaskCreatePinnedToCore(vTaskPass2Mainboard, "pass to main board thread", (1 << 8), NULL, 2, &pass_handle, (UBaseType_t)0);
}

// task for reading data/publishing udp
void vTaskDataProcessing(void* pvParameter)
{

    // sensor init
    ads1120_init_param_t ads1120_params = {.cs_pin = GPIO_NUM_38, .drdy_pin = GPIO_NUM_NC, .spi_host = SPI2_HOST};

    ad5626_init_param_t ad5626_params = {
        .cs_pin = GPIO_NUM_37, .ldac_pin = GPIO_NUM_NC, .clr_pin = GPIO_NUM_NC, .spi_host = SPI2_HOST};

    driveSensorSetup sensors;
    sensors.init(ads1120_params, ad5626_params);

    sensors.setDACValue(dac_bias);

    int array_ct = 0;
    std::array<wsg_data_t, 6> udp_data_buf;

    wsg_data_t* sample = (wsg_data_t*)malloc(sizeof(wsg_data_t));
    sample->dac_bias   = wsg_mem.dac_bias;
    sample->wsg_id     = wsg_mem.wsg_id;

    while (1) {
        sample = new wsg_data_t();
        memset(sample, 0, sizeof(wsg_data_t));

        drive_measurement_t measure;

        // THIS MEASUREMENT DOESN'T WORK LOL
        sensors.measure(true, &measure);

        sample->timestamp = get_timestamp();
        sample->dac_bias  = dac_bias;

        drive_cfg_t drive_cfg;
        drive_cfg.mode = drive_cfg_t::MEASURING_MODE;

        for (int i = 0; i < 3; i++) {
            drive_cfg.channel = SG_CHANNELS[i];
            sensors.configure(drive_cfg);
            sensors.measure(true, &measure);

            sample->sample[i] = measure.adc_value;
        }

        if (xQueueSend(flash_mem_q, sample, 0) != pdPASS) {
            ESP_LOGW(TAG, "failed to add sample to flash mem queue");
        }

        // if (xQueueSend(pass_q, sample, 0) != pdPASS) {
        //     ESP_LOGW(TAG, "failed to add sample to pass queue");
        // }

        if (!uxQueueSpacesAvailable(flash_mem_q)) {
            // Notify the writer to do the writing
            xTaskNotifyGiveIndexed(write_handle, sem_val);
            // // Notify the passer to pass the data to the esp mainboar
            // xTaskNotifyGiveIndexed(pass_handle, sem_val);
        }

        // passing data to the mainboard
        // are we sure this is the right increment?
        memcpy(&udp_data_buf + sizeof(wsg_data_t)*array_ct, sample, sizeof(wsg_data_t));
        array_ct++;

        if (array_ct == 6) {
            serialize_msg_and_publish(udp_data_buf);
            array_ct = 0;
        }
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

        wsg_data_t* sample = new wsg_data_t();

        while (xQueueReceive(flash_mem_q, sample, 10) == pdPASS) {
            // put stuff in write format
            std::vector<uint16_t> buf(sample->sample.begin(), sample->sample.end());
            // delete sample ptr
            delete sample;
            wsg_mem.indiv_write(sample->timestamp, buf);
            // write data to flash in one go or not
        }
    }
}

// // Pass the wsg data onto the main board esp32
// void vTaskPass2Mainboard(void* pvParameter){
//     while (1) {
//         uint32_t notif_val = ulTaskNotifyTakeIndexed(sem_val, pdTRUE, portMAX_DELAY);
//         if (notif_val != 1) {
//             continue;
//         }

//         wsg_data_t* sample = new wsg_data_t();

//         while (xQueueReceive(flash_mem_q, sample, 10) == pdPASS) {
//             // put stuff in write format
//             std::array<uint8_t, 19UL> send_buf = serialize_wsg_data(sample);
//             // delete sample ptr
//             delete sample;
            
//             esp_err_t err = client.publish_data(get_timestamp(), send_buf, send_buf.size());

//             if (err != ESP_OK) {
//                 ESP_LOGW(TAG, "failed to serialize and publish data");
//             }
            
//         }
//     }
// }

esp_err_t serialize_msg_and_publish(std::array<wsg_data_t, 6> data_arr)
{
    std::array<uint8_t, MESSAGE_MAX_LEN> send_data = {0};

    std::array<uint8_t, 19> temp_d;
    for (int i = 0; i < data_arr.size(); i++) {
        temp_d = serialize_wsg_data(data_arr[i]);
        std::copy(temp_d.begin(), temp_d.end(), send_data.begin() + (i * 19));
    }

    esp_err_t err = client.publish_data(get_timestamp(), send_data, send_data.size());

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to serialize and publish data");
    }

    return err;
}