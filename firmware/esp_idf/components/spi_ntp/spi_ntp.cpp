#include <driver/spi_slave.h>
#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <array>

#include "spi_ntp.hpp"

#define US_PER_SECOND 1000000
static const char *TAG = "SPI NTP";

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf) {
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num += buf.at(8) << (i * 8);
    }

    return num;
}

void sub_timeval(struct timeval t1, struct timeval t2, struct timeval *td)
{
    td->tv_usec = t2.tv_usec - t1.tv_usec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_usec < 0)
    {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_usec > 0)
    {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    }
}

void add_timeval(struct timeval t1, struct timeval t2, struct timeval *td)
{
    td->tv_usec = t2.tv_usec + t1.tv_usec;
    td->tv_sec  = t2.tv_sec + t1.tv_sec;
    if (td->tv_usec >= US_PER_SECOND)
    {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    }
    else if (td->tv_usec <= -US_PER_SECOND)
    {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    }
}

void half_timeval(struct timeval * t1) {
    t1->tv_sec /= 2;
    t1->tv_usec /=2;
}

void recv_time(spi_slave_transaction_t * trans) {    
    NTPviaSPI * obj = (NTPviaSPI *) trans->user;

    switch(obj->recv_count) {
        case 0:
            gettimeofday(&(obj->t0), NULL);
            ESP_LOGI(TAG, "Received transmission 1");
            break;
        case 1:
            gettimeofday(&(obj->t3), NULL);
            ESP_LOGI(TAG, "Received transmission 2");
            break;
        case 2:
            ESP_LOGI(TAG, "Received transmission 3");
            break;
        default:
            obj->recv_time_err = ESP_FAIL;
            ESP_LOGE(TAG, "Recv count is invalid: %d", obj->recv_count);
            return;
    }

    obj->recv_count++;
}

NTPviaSPI::NTPviaSPI(spi_host_device_t host) : spi_host(host) {
    spi_bus_config_t settings;
    settings.mosi_io_num = 35;
    settings.miso_io_num = 24;
    settings.sclk_io_num = 33;

    spi_slave_interface_config_t slave_config;
    slave_config.spics_io_num = 32;
    slave_config.flags = 0;
    slave_config.queue_size = 4;
    slave_config.mode = 0;
    slave_config.post_setup_cb = &recv_time;

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &settings, &slave_config, SPI_DMA_DISABLED);
    switch(err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Initialized spi slave interface");
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "Configuration is invalid");
            break;
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(TAG, "Host is already in use");
            break;
        case ESP_ERR_NOT_FOUND:
            ESP_LOGE(TAG, "No available DMA channel");
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memeory");
            break;
    }

}

esp_err_t NTPviaSPI::sync() {
    std::array<uint8_t, 1> dummy_buf = {0};
    std::array<uint8_t, 8> rx_buf_trans1;
    std::array<uint8_t, 8> rx_buf_trans2;
    std::array<uint8_t, 8> rx_buf_trans3;

    spi_slave_transaction_t trans1;
    spi_slave_transaction_t * ptrans1 = &trans1;
    trans1.flags = 0;
    trans1.length = dummy_buf.size() << 3;
    trans1.tx_buffer = dummy_buf.data();
    trans1.rx_buffer = rx_buf_trans1.data();
    trans1.user = this;


    spi_slave_transaction_t trans2;
    spi_slave_transaction_t * ptrans2 = &trans2;
    trans2.flags = 0;
    trans2.length = dummy_buf.size() << 3;
    trans2.tx_buffer = dummy_buf.data();
    trans2.rx_buffer = rx_buf_trans2.data();
    trans2.user = this;

    spi_slave_transaction_t trans3;
    spi_slave_transaction_t * ptrans3 = &trans3;
    trans3.flags = 0;
    trans3.length = dummy_buf.size() << 3;
    trans3.tx_buffer = dummy_buf.data();
    trans3.rx_buffer = rx_buf_trans3.data();
    trans3.user = this;

    esp_err_t err = spi_slave_queue_trans(spi_host, ptrans1, portMAX_DELAY);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed queuing first transaction");
        return err;
    }

    err = spi_slave_queue_trans(spi_host, ptrans2, portMAX_DELAY);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed queuing second transaction");
        return err;
    }

    err = spi_slave_queue_trans(spi_host, ptrans3, portMAX_DELAY);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed queuing third transaction");
        return err;
    }

    err = spi_slave_get_trans_result(spi_host, &ptrans1, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finish first transaction");
        return err;
    }

    err = spi_slave_get_trans_result(spi_host, &ptrans2, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finish second transaction");
        return err;
    }

    err = spi_slave_get_trans_result(spi_host, &ptrans3, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to finish third transaction");
        return err;
    }

    if(recv_time_err != ESP_OK) {
        ESP_LOGE(TAG, "Error with transmission");
        return recv_time_err;
    }

    uint64_t timestamp_trans2 = buf_to_uint64(rx_buf_trans2);
    uint64_t timestamp_trans3 = buf_to_uint64(rx_buf_trans3);

    t2.tv_usec = timestamp_trans2 % 10000000L;
    t2.tv_sec = (int64_t) (timestamp_trans2 / 1000000L);

    t1.tv_usec = timestamp_trans3 % 10000000L;
    t1.tv_sec = (int64_t) (timestamp_trans3 / 1000000L);

    struct timeval sub1;
    struct timeval sub2;
    struct timeval offset;
    struct timeval cur_time;

    sub_timeval(t1, t0, &sub1);
    sub_timeval(t2, t3, &sub2);
    add_timeval(sub1, sub2, &offset);
    half_timeval(&offset);

    gettimeofday(&cur_time, NULL);
    sub_timeval(offset, cur_time, &cur_time);
    ESP_LOGI(TAG, "Err: %d", settimeofday(&cur_time, NULL));
    ESP_LOGI(TAG, "Seconds: %lld, Microseconds: %lld", (int64_t) cur_time.tv_sec, (int64_t) cur_time.tv_usec);

    return ESP_OK;
}
