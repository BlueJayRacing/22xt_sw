#include "spi_ntp.hpp"
#include "esp_timer.h"
#include <array>

#include <driver/spi_slave.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/freertos.h>
#include <stdio.h>

#define US_PER_SECOND 1000000
static const char* TAG = "SPI NTP";

uint64_t buf_to_uint64(std::array<uint8_t, 8> buf)
{
    uint64_t num = 0;
    for (int i = 0; i < 8; i++) {
        num |= ((uint64_t)buf[i] << (i * 8));
    }
    return num;
}

void sub_timeval(struct timeval t1, struct timeval t2, struct timeval* td)
{
    td->tv_usec = t2.tv_usec - t1.tv_usec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_usec < 0) {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    } else if (td->tv_sec < 0 && td->tv_usec > 0) {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    }
}

void add_timeval(struct timeval t1, struct timeval t2, struct timeval* td)
{
    td->tv_usec = t2.tv_usec + t1.tv_usec;
    td->tv_sec  = t2.tv_sec + t1.tv_sec;
    if (td->tv_usec >= US_PER_SECOND) {
        td->tv_usec -= US_PER_SECOND;
        td->tv_sec++;
    } else if (td->tv_usec <= -US_PER_SECOND) {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    }
}

void half_timeval(struct timeval* t1)
{
    t1->tv_sec /= 2;
    t1->tv_usec /= 2;
}

void IRAM_ATTR recv_time(spi_slave_transaction_t* trans)
{
    NTPviaSPI* obj = (NTPviaSPI*)trans->user;
    int64_t now_us = esp_timer_get_time();

    switch (obj->recv_count) {
    case 0:
        // gettimeofday(&(obj->t0), NULL);
        // ESP_LOGI(TAG, "Received transmission 1");
        obj->t0.tv_sec  = now_us / 1000000;
        obj->t0.tv_usec = now_us % 1000000;
        break;
    case 1:
        // gettimeofday(&(obj->t3), NULL);
        // ESP_LOGI(TAG, "Received transmission 2");
        obj->t3.tv_sec  = now_us / 1000000;
        obj->t3.tv_usec = now_us % 1000000;
        break;
    case 2:
        // ESP_LOGI(TAG, "Received transmission 3");
        break;
    default:
        obj->recv_time_err = ESP_FAIL;
        // ESP_LOGE(TAG, "Recv count is invalid: %d", obj->recv_count);
        return;
    }

    obj->recv_count++;
}
void my_post_setup_cb(spi_slave_transaction_t* trans) { return; }
NTPviaSPI::NTPviaSPI(spi_host_device_t host) : spi_host(host)
{
    spi_bus_config_t settings = {};
    spi_slave_interface_config_t slave_config = {}; 
    settings.mosi_io_num      = 18;
    settings.miso_io_num      = 20;
    settings.sclk_io_num      = 19;
    slave_config.spics_io_num = 21;

    slave_config.flags         = 0;
    slave_config.queue_size    = 4;
    slave_config.mode          = 0;
    slave_config.post_setup_cb = my_post_setup_cb;

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &settings, &slave_config, SPI_DMA_DISABLED);
    switch (err) {
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

esp_err_t NTPviaSPI::sync()
{
    ESP_LOGI(TAG, "Sync began");
    recv_count                       = 0;
    recv_time_err                    = ESP_OK;
    // std::array<uint8_t, 1> dummy_buf = {0};
    // std::array<uint8_t, 8> rx_buf_trans1;
    // std::array<uint8_t, 8> rx_buf_trans2;
    // std::array<uint8_t, 8> rx_buf_trans3;

    // member variables to configure transactions
    std::array<char*, 3> sendbuf; 
    sendbuf.fill(static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0))); 
    std::array<char*, 3> recvbuf; 
    recvbuf.fill(static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0))); 
    // char* sendbuf = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0));
    // char* recvbuf = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0));

    for (int i = 0; i < 3; i++) {
        memset(recvbuf[i], 0xA5, 9);
        memset(sendbuf[i], 0x01, 9);
        memset(&trans[i], 0, sizeof(spi_slave_transaction_t*));
        trans[i]->flags     = 0;
        trans[i]->length    = 8 * 8;
        trans[i]->trans_len = 8 * 8;
        dummy_tx_buf[0]     = 0x01;
        // trans[i].tx_buffer = dummy_tx_buf.data();
        // trans[i].rx_buffer = rx_buf[i].data();
        trans[i]->tx_buffer = sendbuf[i];
        trans[i]->rx_buffer = recvbuf[i];
        trans[i]->user      = this;
    }

    // spi_slave_transaction_t trans1;
    // spi_slave_transaction_t* ptrans1 = &trans1;
    // memset(ptrans1, 0, sizeof(spi_slave_transaction_t));
    // trans1.flags     = 0;
    // trans1.length    = dummy_buf.size() << 3;
    // trans1.tx_buffer = dummy_buf.data();
    // trans1.rx_buffer = rx_buf_trans1.data();
    // trans1.user      = this;

    // spi_slave_transaction_t trans2;
    // spi_slave_transaction_t* ptrans2 = &trans2;
    // memset(ptrans2, 0, sizeof(spi_slave_transaction_t));
    // trans2.flags     = 0;
    // trans2.length    = dummy_buf.size() << 3;
    // trans2.tx_buffer = dummy_buf.data();
    // trans2.rx_buffer = rx_buf_trans2.data();
    // trans2.user      = this;

    // spi_slave_transaction_t trans3;
    // spi_slave_transaction_t* ptrans3 = &trans3;
    // memset(ptrans3, 0, sizeof(spi_slave_transaction_t));
    // trans3.flags     = 0;
    // trans3.length    = dummy_buf.size() << 3;
    // trans3.tx_buffer = dummy_buf.data();
    // trans3.rx_buffer = rx_buf_trans3.data();
    // trans3.user      = this;

    // queue and wait for transaction 0
    esp_err_t err10 = spi_slave_queue_trans(spi_host, trans[0], 2000);
    if (err10 != ESP_OK) {
        ESP_LOGI(TAG, "Failed queuing transaction %d", 0);
        return err10;
    }
    vTaskDelay(10);

    // ESP_LOGI(TAG, "Successfully queued transaction %d", 0);

    spi_slave_transaction_t* out_trans;
    esp_err_t err00 = spi_slave_get_trans_result(spi_host, &out_trans, 2000);
    ESP_LOGI(TAG, "check");
    if (err00 == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Timed out waiting for Master on transaction %d", 0);
        return err00;
    } else if (err00 != ESP_OK) {
        ESP_LOGE(TAG, "SPI Error: %s", esp_err_to_name(err00));
        return err00;
    }
    uint8_t* data = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "Transaction %d completed! First byte: %02X", 0, data[0]);

    // // queue and wait for transaction 1
    // esp_err_t err11 = spi_slave_queue_trans(spi_host, trans[1], portMAX_DELAY);
    // if (err11 != ESP_OK) {
    //     ESP_LOGI(TAG, "Failed queuing transaction %d", 1);
    //     return err11;
    // }
    // esp_err_t err01 = spi_slave_get_trans_result(spi_host, &out_trans, portMAX_DELAY);
    // if (err01 == ESP_ERR_TIMEOUT) {
    //     ESP_LOGE(TAG, "Timed out waiting for Master on transaction %d", 1);
    //     return err01;
    // } else if (err01 != ESP_OK) {
    //     ESP_LOGE(TAG, "SPI Error: %s", esp_err_to_name(err01));
    //     return err01;
    // }
    // ESP_LOGI(TAG, "Transaction %d completed!", 1);

    // esp_err_t err = spi_slave_queue_trans(spi_host, trans[1], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed queuing first transaction");
    //     return err;
    // }

    // err = spi_slave_queue_trans(spi_host, trans[2], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed queuing second transaction");
    //     return err;
    // }

    // err = spi_slave_queue_trans(spi_host, trans[3], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed queuing third transaction");
    //     return err;
    // }

    // err = spi_slave_get_trans_result(spi_host, &trans[1], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to finish first transaction");
    //     return err;
    // }

    // err = spi_slave_get_trans_result(spi_host, &trans[2], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to finish second transaction");
    //     return err;
    // }

    // err = spi_slave_get_trans_result(spi_host, &trans[3], portMAX_DELAY);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to finish third transaction");
    //     return err;
    // }

    if (recv_time_err != ESP_OK) {
        ESP_LOGI(TAG, "Error with transmission");
        return recv_time_err;
    }

    uint64_t timestamp_trans2 = buf_to_uint64(rx_buf[1]);
    uint64_t timestamp_trans3 = buf_to_uint64(rx_buf[2]);

    t2.tv_usec = timestamp_trans2 % 10000000L;
    t2.tv_sec  = (int64_t)(timestamp_trans2 / 1000000L);

    t1.tv_usec = timestamp_trans3 % 10000000L;
    t1.tv_sec  = (int64_t)(timestamp_trans3 / 1000000L);

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
    ESP_LOGI(TAG, "Seconds: %lld, Microseconds: %lld", (int64_t)cur_time.tv_sec, (int64_t)cur_time.tv_usec);

    return ESP_OK;
}
