#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <spi_ntp.hpp>
#include <stdio.h>

static const char* TAG = "main";

extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    NTPviaSPI* spi_sync = new NTPviaSPI(SPI2_HOST);
    // WORD_ALIGNED_ATTR uint8_t rx_buf[8]      = {0};
    // WORD_ALIGNED_ATTR uint8_t dummy_tx_buf[] = {0x01, 0, 0, 0, 0, 0, 0, 0};
    // spi_slave_transaction_t t;
    // memset(&t, 0, sizeof(t));
    // esp_err_t ret;
    // int n = 0;
    // while (1) {
    //     // Clear receive buffer, set send buffer to something sane
    //     memset(rx_buf, 0, 8 * sizeof(uint8_t));

    //     // Set up a transaction of 1 bytes to send/receive
    //     t.length    = 8 * 8;
    //     t.tx_buffer = dummy_tx_buf;
    //     t.rx_buffer = rx_buf;
    //     /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
    //     initialized by the SPI master, however, so it will not actually happen until the master starts a hardware
    //     transaction by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line,
    //     pulled up by the .post_setup_cb callback that is called as soon as a transaction is ready, to let the master
    //     know it is free to transfer data.
    //     */
    //     ret = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);

    //     // spi_slave_transmit does not return until the master has done a transmission, so by here we have sent our
    //     data
    //     // and received data from the master. Print it.
    //     printf("Received: %i %i %i\n", rx_buf[0], rx_buf[1], rx_buf[2]);
    //     // struct timeval tv;
    //     // gettimeofday(&tv, NULL);
    //     // ESP_LOGI(TAG, "ESP: %lld.%06lld", (int64_t)tv.tv_sec, (int64_t)tv.tv_usec);
    //     n++;
    // }
    bool not_comp = true;
    ;
    while (not_comp) {
        esp_err_t err = spi_sync->sync();
        if (err == ESP_OK) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ESP_LOGI(TAG, "ESP: %lld.%06lld", (int64_t)tv.tv_sec, (int64_t)tv.tv_usec);
            not_comp -= false;
        } else {
            ESP_LOGI(TAG, "Failed to sync: %d", err);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
