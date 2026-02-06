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

// Goals:
// teensy sends "opcodes" to the esp to run arbrirary functions (this gives the option to extend what the esp can do)
// When sent a "start reading" command from the teensy, open the server to the wsgs and read from them.
static const char* TAG = "main";
// templates
void startup(void);
esp_err_t functional_loop(spi_host_device_t spi_host);
esp_err_t wsg_read_pass(spi_host_device_t spi_host, uint8_t num_wsg);
esp_err_t calibrate_wsg(spi_host_device_t spi_host, uint8_t num_wsg);
extern "C" void app_main(void) { startup(); }

void startup(void)
{
    // Choose spi host
    spi_host_device_t spi_host = SPI1_HOST;

    // Time sync with the teensy
    // SPI also gets initialized via this
    NTPviaSPI spi_sync = NTPviaSPI(spi_host);
    esp_err_t err      = spi_sync.sync();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Sync completed succesfully");
    } else {
        ESP_LOGE(TAG, "Failed to sync: %d", err);
    }

    // start transmission loop for teensy to call functions
    err = functional_loop(spi_host);

} // end startup

/* ---------------------------------------
    Loop for the teensy to call functions on the esp using the correct opcode transmitted through SPI.
    - 0x04: wsg read-pass loop
    - 0x05: wsg calibration
    - 0xFF: Stop esp32
--------------------------------------- */
esp_err_t functional_loop(spi_host_device_t spi_host)
{
    esp_err_t err;
    bool run_while = true;
    while (run_while) {
        std::array<uint8_t, 1> dummy_buf = {0};
        std::array<uint8_t, 1> rx_buf_opcode;
        spi_slave_transaction_t teensy_optrans;
        spi_slave_transaction_t* pteensy_optrans = &teensy_optrans;
        teensy_optrans.flags                     = 0;
        teensy_optrans.length                    = dummy_buf.size() << 3;
        teensy_optrans.tx_buffer                 = dummy_buf.data();
        teensy_optrans.rx_buffer                 = rx_buf_opcode.data(); // storing an 8-bit opcode
        teensy_optrans.user                      = NULL;

        // Get the opcode from the teensy
        err = spi_slave_transmit(spi_host, pteensy_optrans, portMAX_DELAY);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SPI transmitted succesfully");
        } else {
            ESP_LOGE(TAG, "Failed SPI: %d", err);
        }

        // switch based on the opcode to call some function
        switch (rx_buf_opcode[0]) {
        case 0x04: // Read from udp the wsg and then pass on through spi
            ESP_LOGI(TAG, "Starting wsg read-pass");
            wsg_read_pass(spi_host, 1);
            break;
        case 0x05: // Read from udp the wsg and then pass on through spi
            ESP_LOGI(TAG, "Starting calibration");
            // TODO: add calibration function
            break;

        case 0xFF:
            run_while = false;
            break;

        default: // Wait a bit if nothing happened
            vTaskDelay(pdMS_TO_TICKS(10));
            break;
        }
    }
    return err;
} // end functional loop

// Read data from the wsgs and pass it to the teensy through spi
esp_err_t wsg_read_pass(spi_host_device_t spi_host, uint8_t num_wsg)
{
    UdpServer server;
    esp_err_t err = server.initialize_wifi_connection();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WIFI init failed %d", err);
        return err;
    }
    err = server.initialize_socket();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "socket init failed %d", err);
        return err;
    }

    // TODO: Time sync with the wsgs
    start_server_timesync_loop(); //?is this correct?

    // Start reading and passing from the wsgs
    ESP_LOGI(TAG, "Starting communication with wsg and teensy");
    while (1) {

        // TODO: add the udp "read" part here

        Message* msg = server.recv_data();
        if (msg == nullptr) {
            continue;
        }

        // "pass" onto the teensy
        // data is already serialized when recieved so we don't need to serialize again

        std::array<uint8_t, 1> rx_buf;
        spi_slave_transaction_t wsg_trans;
        spi_slave_transaction_t* pwsg_trans = &wsg_trans;
        wsg_trans.flags                     = 0;
        wsg_trans.length                    = msg->payload_len << 3;
        wsg_trans.tx_buffer                 = static_cast<void*>(&(msg->payload[0])); // Storing the wsg_datas
        wsg_trans.rx_buffer = rx_buf.data(); // Stores any signals from the teensy, e.g. 0xFF stop signal.
        wsg_trans.user      = NULL;

        err = spi_slave_transmit(spi_host, pwsg_trans, portMAX_DELAY); // Transmit the wsg to the teensy
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed WSG pass: %d", err);
        }
        if (rx_buf[0] == 0xFF) { // Stop signal from the teensy
            break;
        }
    }
    return err;
} // end wsg_read_pass
