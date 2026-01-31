#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <spi_ntp.hpp>
#include <spi_slave.h>
#include <stdio.h>
#include <udp_server.hpp>


// Goals: 
// teensy sends "opcodes" to the esp to run arbrirary functions (this gives the option to extend what the esp can do)
// When sent a "start reading" command from the teensy, open the server to the wsgs and read from them.
static const char* TAG = "main";
// templates
void startup(void);
esp_err_t wsg_read_pass();

extern "C" void app_main(void)
{
    startup();
}

void startup(void){
    // Choose spi host
    spi_host_device_t spi_host = SPI1_HOST;
    
    // Time sync with the teensy
    // SPI also gets initialized via this
    NTPviaSPI spi_sync = NTPviaSPI(spi_host);
    esp_err_t err = spi_sync.sync();
    if(err == ESP_OK) {
        ESP_LOGI(TAG, "Sync completed succesfully");
    } else {
        ESP_LOGE(TAG, "Failed to sync: %d", err);
    }

    // Wait for transmission from the teensy
    bool run_while = true;
    while (run_while){
        std::array<uint8_t, 1> dummy_buf = {0};
        std::array<uint8_t, 1> rx_buf_opcode;
        spi_slave_transaction_t teensy_optrans;
        spi_slave_transaction_t * pteensy_optrans = &teensy_optrans;
        trans3.flags = 0;
        trans3.length = dummy_buf.size() << 3;
        trans3.tx_buffer = dummy_buf.data();
        trans3.rx_buffer = rx_buf_opcode.data(); //storing an 8-bit opcode
        trans3.user = NULL;
        
        // Get the opcode from the teensy
        esp_err_t err = spi_slave_transmit(spi_host, pteensy_optrans, portMAX_DELAY);
        if (err == ESP_OK){
            ESP_LOGI(TAG, "SPI transmitted succesfully");
        } else {
            ESP_LOGE(TAG, "Failed SPI: %d", err);
        }

        // switch based on the opcode to call some function
        switch (rx_buf_opcode[0]) {
        case 4: // Read from udp the wsg and then pass on through spi
            ESP_LOGI(TAG, "Starting wsg read-pass");
            uint8_t num_wsg = 1;
            wsg_read_pass(spi_host, num_wsg);
        break;

        case 0xFF:
            run_while = false;
        break;
        
        default: // Wait a bit if nothing happened
            vTaskDelay(pdMS_TO_TICKS(10));
        break;
        }
    }


} //end startup

// Read data from the wsgs and pass it to the teensy through spi
esp_err_t wsg_read_pass(spi_host_device_t spi_host, uint8_t num_wsg){
    UdpServer server;
    server.initialize_wifi_connection();
    server.initialize_socket();

    //TODO: Time sync with the wsgs
    start_server_timesync_loop();




    // Start reading and passing from the wsgs
    Message * msg = server.recv_data();
        if (msg != nullptr) {
            //TODO: Deal with the msg
        }


}