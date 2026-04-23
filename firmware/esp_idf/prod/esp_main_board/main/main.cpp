/* ---------------------------------------
Description: A block to control the main board
    esp32. basically a communication peripheral
    to talk between the wsg and teensy.

    On startup the teensy can send some
    opcodes to the esp32 to do certain
    functions.
--------------------------------------- */
#include <driver/spi_slave.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <ntp_udp_server.h>
#include <spi_ntp.hpp>
#include <stdio.h>
#include <udp_server.hpp>

// #define NO_TEENSY true

gpio_num_t handshake_pin = GPIO_NUM_3;
#define SPI_SIZE 116

typedef struct {
    NTPviaSPI sync_host;
    spi_host_device_t spi_host;
} main_thread_params_t;

// Goals:
// teensy sends "opcodes" to the esp to run arbrirary functions (this gives the option to extend what the esp can do)
// When sent a "start reading" command from the teensy, open the server to the wsgs and read from them.
static const char* TAG = "main";
// templates
void startup(void);
void functional_loop(spi_host_device_t spi_host, NTPviaSPI spi_sync);
void wsg_read_pass(void * pvParams);
esp_err_t calibrate_wsg(spi_host_device_t spi_host, uint8_t num_wsg);

extern "C" void app_main(void) { startup(); }

void startup(void)
{
    // Choose spi host
    spi_host_device_t spi_host = SPI2_HOST; // 2 instead of 1 bc on website says we should generally use not 1
    esp_err_t err;

#ifndef NO_TEENSY
    // Time sync with the teensy
    // SPI also gets initialized via this
    // err      = spi_sync.sync();
    // if (err == ESP_OK) {
    //     ESP_LOGI(TAG, "Sync completed succesfully");
    // } else {
    //     ESP_LOGE(TAG, "Failed to sync: %d", err);
    // }
#endif

    // start transmission loop for teensy to call functions
    // wsg_read_pass(spi_host, 0, spi_sync);
    xTaskCreate(wsg_read_pass, "data sending thread", 1<<12, (void *)&spi_host, 3, NULL);
    vTaskDelete( NULL );
} // end startup

/* ---------------------------------------
    Loop for the teensy to call functions on the esp using the correct opcode transmitted through SPI.
    - 0x04: wsg read-pass loop
    - 0x05: wsg calibration
    - 0xFF: Stop esp32
--------------------------------------- */
// void functional_loop(spi_host_device_t spi_host, NTPviaSPI spi_sync)
// {
//     esp_err_t err  = ESP_OK;
//     bool run_while = true;

//     return ESP_OK;
//     while (run_while) {

// #ifndef NO_TEENSY
//         std::array<uint8_t, 1> dummy_buf = {0};
//         std::array<uint8_t, 1> rx_buf_opcode;
//         spi_slave_transaction_t teensy_optrans;
//         spi_slave_transaction_t* pteensy_optrans = &teensy_optrans;
//         teensy_optrans.flags                     = 0;
//         teensy_optrans.length                    = dummy_buf.size() << 3;
//         teensy_optrans.tx_buffer                 = dummy_buf.data();
//         teensy_optrans.rx_buffer                 = rx_buf_opcode.data(); // storing an 8-bit opcode
//         teensy_optrans.user                      = NULL;

//         ESP_LOGI(TAG, "check 1");

//         // Get the opcode from the teensy
//         err = spi_slave_transmit(spi_host, pteensy_optrans, portMAX_DELAY);
//         if (err == ESP_OK) {
//             ESP_LOGI(TAG, "SPI transmitted succesfully");
//         } else {
//             ESP_LOGE(TAG, "Failed SPI: %d", err);
//         }

//         ESP_LOGI(TAG, "check 2");

//         // switch based on the opcode to call some function
//         switch (rx_buf_opcode[0]) {
//         case 0x04: // Read from udp the wsg and then pass on through spi
//             ESP_LOGI(TAG, "Starting wsg read-pass");
//             wsg_read_pass(spi_host, 1);
//             break;
//         case 0x05: // Calibrate the strain gauges.
//             ESP_LOGI(TAG, "Starting calibration");
//             // TODO: add calibration function
//             break;

//         case 0xFF:
//             run_while = false;
//             break;

//         default: // Wait a bit if nothing happened
//             vTaskDelay(pdMS_TO_TICKS(1));
//             break;
//         }
//     }
//     return err;
// #else
//         wsg_read_pass(spi_host, 1);
//         // esp_task_wdt_reset();
//     }
//     return ESP_OK;
// }

// Read data from the wsgs and pass it to the teensy through spi
// also does time sync with wsgs
void wsg_read_pass(void * pvParams)
{

    spi_host_device_t spi_host = SPI2_HOST;
    NTPviaSPI spi_sync = NTPviaSPI(spi_host);
    UdpServer server;
    esp_err_t err = server.initialize_wifi_connection();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WIFI init failed %d", err);
    }
    err = server.initialize_socket();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "socket init failed %d", err);
    }

    gpio_set_direction(handshake_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handshake_pin, 0);

    start_server_timesync_loop();
    taskYIELD();

    ESP_LOGI(TAG, "Starting communication with wsg and teensy");

    // std::array<uint8_t, 8> test_payload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    // std::array<uint8_t, 8> rx_buf = {0};

    WORD_ALIGNED_ATTR uint8_t * sendbuf;
    WORD_ALIGNED_ATTR uint8_t * recvbuf;

    sendbuf = static_cast<uint8_t *>(spi_bus_dma_memory_alloc(SPI2_HOST, SPI_SIZE, 0));
    recvbuf = static_cast<uint8_t *>(spi_bus_dma_memory_alloc(SPI2_HOST, SPI_SIZE, 0));

    assert(sendbuf != nullptr);
    assert(recvbuf != nullptr);

    memset(sendbuf, 0, SPI_SIZE);
    memset(recvbuf, 0, SPI_SIZE);

    // for (int i = 0; i < SPI_SIZE; i++) {
    //     sendbuf[i] = i;
    // }

    Message * msg;
    while (1) {
        // ESP_LOGI(TAG, "STARTED LOOP");
        msg = server.recv_data();
        if (msg == nullptr) {
            // vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        // ESP_LOGI(TAG, "receved bytes:");
        // for (int i = 0; i < msg->payload_len; i++) {
        //     ESP_LOGI(TAG, "byte %d: %02x", i, msg->payload[i]);
        // }

#ifndef NO_TEENSY
        memset(sendbuf, 0, SPI_SIZE);
        memcpy(sendbuf, msg->payload.data(), SPI_SIZE < msg->payload_len ? SPI_SIZE : msg->payload_len);
        spi_slave_transaction_t wsg_trans = {};
        wsg_trans.length = SPI_SIZE << 3;
        wsg_trans.tx_buffer = sendbuf;
        wsg_trans.rx_buffer = recvbuf;

        // for (int i = 0; i < SPI_SIZE; i++) {
        //     ESP_LOGI(TAG, "Byte %d: %d", i, ((uint8_t *) wsg_trans.tx_buffer)[i]);
        //     assert(((uint8_t *) wsg_trans.tx_buffer)[i] == 1);
        // }

        spi_slave_queue_trans(spi_host, &wsg_trans, portMAX_DELAY);

        gpio_set_level(handshake_pin, 1);
        // ESP_LOGI(TAG, "Handshake HIGH - waiting for Teensy");

        spi_slave_transaction_t* result;
        spi_slave_get_trans_result(spi_host, &result, portMAX_DELAY);

        gpio_set_level(handshake_pin, 0);
        uint8_t * data = (uint8_t *) result->rx_buffer;
        ESP_LOGI(TAG, "Sent data to Teensy and recv: %02x", data[0]);
        if (data[0] == 0x88) {
            spi_sync.sync();
        }
        // vTaskDelay(5);
#endif
        delete msg;
    }


// #ifndef NO_TEENSY
//         std::array<uint8_t, MESSAGE_MAX_LEN> rx_buf;
//         spi_slave_transaction_t wsg_trans   = {0};
//         spi_slave_transaction_t* pwsg_trans = &wsg_trans;
//         wsg_trans.flags                     = 0;
//         wsg_trans.length                    = msg->payload_len << 3;
//         wsg_trans.tx_buffer                 = &(msg->payload[0]); // Storing the wsg_datas
//         wsg_trans.rx_buffer = rx_buf.data(); // Stores any signals from the teensy, e.g. 0xFF stop signal.
//         wsg_trans.user      = NULL;

//         err = spi_slave_transmit(spi_host, pwsg_trans, portMAX_DELAY); // Transmit the wsg to the teensy
//         if (err != ESP_OK) {
//             ESP_LOGE(TAG, "Failed WSG pass: %d", err);
//         }
//         if (rx_buf[0] == 0xFF) { // Stop signal from the teensy
//             break;
//         }

//         gpio_set_level(handshake_pin, 1);
//         ESP_LOGI(TAG, "Handshake HIGH - waiting for Teensy");

//         spi_slave_transaction_t* result;
//         err = spi_slave_get_trans_result(spi_host, &result, portMAX_DELAY);
//         gpio_set_level(handshake_pin, 0);  // lower handshake after transfer

//         if (err != ESP_OK) {
//             ESP_LOGE(TAG, "Failed WSG pass: %d", err);
//         }
//         if (rx_buf[0] == 0xFF) {  // Stop signal from Teensy
//             delete msg;
//             break;
//         }
// #else
//         ESP_LOGI(TAG, "Received data from wsg (%d): %s", msg->payload_len, msg->payload.data());
// #endif
//         delete msg;
//     }
//     return err;
} // end wsg_read_pass
