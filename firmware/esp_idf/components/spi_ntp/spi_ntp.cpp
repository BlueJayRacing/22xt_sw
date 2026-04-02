#include "spi_ntp.hpp"
#include "esp_timer.h"
#include <array>

#include "driver/gpio.h"
#include <driver/spi_slave.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>

#define US_PER_SECOND 1000000
static const char* TAG   = "SPI NTP";
gpio_num_t handshake_pin = GPIO_NUM_2; 

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
    if (td->tv_usec < 0) {
        td->tv_usec += US_PER_SECOND;
        td->tv_sec--;
    } else if (td->tv_usec >= US_PER_SECOND) {
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

void recv_time(spi_slave_transaction_t* trans)
{
    // ESP_LOGI(TAG, "recv_time"); 
    // ESP_LOGI(TAG, "%d", trans->length); 
    gpio_set_level(handshake_pin, 1);
    NTPviaSPI* obj = (NTPviaSPI*)trans->user;
    int64_t now_us = esp_timer_get_time();

    switch (obj->recv_count) {
    case 0:
        // ESP_LOGI(TAG, "Received pre-transmission"); 
        break; 
    case 1:
        // gettimeofday(&(obj->t0), NULL);
        // ESP_LOGI(TAG, "Received transmission 0");
        obj->t0.tv_sec  = now_us / 1000000;
        obj->t0.tv_usec = now_us % 1000000;
        break;
    case 2:
        // gettimeofday(&(obj->t3), NULL);
        // ESP_LOGI(TAG, "Received transmission 1");
        obj->t3.tv_sec  = now_us / 1000000;
        obj->t3.tv_usec = now_us % 1000000;
        break;
    case 3:
        // ESP_LOGI(TAG, "Received transmission 2");
        break;
    default:
        obj->recv_time_err = ESP_FAIL;
        // ESP_LOGE(TAG, "Recv count is invalid: %d", obj->recv_count);
        return;
    }

    obj->recv_count++;
}

void my_post_setup_cb(spi_slave_transaction_t* trans) { gpio_set_level(handshake_pin, 1); }
void my_post_trans_cb(spi_slave_transaction_t* trans) { gpio_set_level(handshake_pin, 0); }

NTPviaSPI::NTPviaSPI(spi_host_device_t host) : spi_host(host)
{
    spi_bus_config_t settings                 = {};
    spi_slave_interface_config_t slave_config = {};

    // c6 pins 
    settings.mosi_io_num                      = 18;
    settings.miso_io_num                      = 20;
    settings.sclk_io_num                      = 19;
    slave_config.spics_io_num                 = 21;

    // s3 pins 
    // settings.mosi_io_num                      = 9;
    // settings.miso_io_num                      = 8;
    // settings.sclk_io_num                      = 7;
    // slave_config.spics_io_num                 = 4;

    slave_config.flags         = 0;
    slave_config.queue_size    = 4;
    slave_config.mode          = 0;
    slave_config.post_setup_cb = recv_time;
    slave_config.post_trans_cb = my_post_trans_cb;

    gpio_config_t handshake_cfg = {};
    handshake_cfg.pin_bit_mask = (1ULL << handshake_pin);
    handshake_cfg.mode         = GPIO_MODE_OUTPUT;
    handshake_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    handshake_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    handshake_cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&handshake_cfg);
    gpio_set_level(handshake_pin, 0);

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &settings, &slave_config, SPI_DMA_CH_AUTO);
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
    recv_count    = 0;
    recv_time_err = ESP_OK;

    // std::array<uint8_t, 8> rx_buf_trans2;
    // std::array<uint8_t, 8> rx_buf_trans3;

    // member variables to configure transactions
    std::array<char*, 3> sendbuf;
    std::array<char*, 3> recvbuf;

    for (int i = 0; i < 3; i++) {
        sendbuf[i] = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 8, 0));
        recvbuf[i] = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 8, 0));

        assert(sendbuf[i] != nullptr);
        assert(recvbuf[i] != nullptr);
    }
    // char* sendbuf = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0));
    // char* recvbuf = static_cast<char*>(spi_bus_dma_memory_alloc(SPI2_HOST, 9, 0));

    for (int i = 0; i < 3; i++) {
        trans[i] = new spi_slave_transaction_t;
        memset(recvbuf[i], 0xA5, 8);
        memset(sendbuf[i], 0x01, 8);
        memset(trans[i], 0, sizeof(spi_slave_transaction_t));
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

    std::array<uint8_t, 1> dummy_buf = {0x01};
    std::array<uint8_t, 8> rx_buf_trans_pre;
    spi_slave_transaction_t trans_pre;
    memset(&trans_pre, 0, sizeof(spi_slave_transaction_t));
    trans_pre.flags     = 0;
    trans_pre.length    = dummy_buf.size() << 3;
    trans_pre.tx_buffer = dummy_buf.data();
    trans_pre.rx_buffer = rx_buf_trans_pre.data();
    trans_pre.user      = this;

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

    // queue all transactions upfront
    spi_slave_queue_trans(spi_host, &trans_pre, portMAX_DELAY);
    esp_err_t err00 = spi_slave_queue_trans(spi_host, trans[0], portMAX_DELAY);
    esp_err_t err10 = spi_slave_queue_trans(spi_host, trans[1], portMAX_DELAY);
    esp_err_t err20 = spi_slave_queue_trans(spi_host, trans[2], portMAX_DELAY);

    // pre-sync transaction 
    spi_slave_transaction_t* out_trans;
    esp_err_t err = spi_slave_get_trans_result(spi_host, &out_trans, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Transaction failed: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t* d = (uint8_t*)trans_pre.tx_buffer;
    ESP_LOGI(TAG, "pre-sync done");

    //transaction 0
    if (err00 != ESP_OK) {
        ESP_LOGI(TAG, "Failed queuing transaction %d", 0);
        return err00;
    }
    esp_err_t err01 = spi_slave_get_trans_result(spi_host, &out_trans, portMAX_DELAY);
    if (err01 == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Timed out waiting for Master on transaction 0");
        return err01;
    } else if (err01 != ESP_OK) {
        ESP_LOGE(TAG, "SPI Error: %s", esp_err_to_name(err01));
        return err01;
    }
    uint8_t* data = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "Transaction 0 completed! First byte: %02X", data[0]);
    d = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "trans[0] result: %02X %02X %02X %02X %02X %02X %02X %02X",
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

    // transaction 1
    if (err10 != ESP_OK) {
        ESP_LOGI(TAG, "Failed queuing transaction 1");
        return err10;
    }
    esp_err_t err11 = spi_slave_get_trans_result(spi_host, &out_trans, portMAX_DELAY);
    if (err11 == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Timed out waiting for Master on transaction 1");
        return err11;
    } else if (err11 != ESP_OK) {
        ESP_LOGE(TAG, "SPI Error: %s", esp_err_to_name(err11));
        return err11;
    }
    uint64_t timestamp_trans3 = buf_to_uint64(*(std::array<uint8_t,8>*)out_trans->rx_buffer);
    data = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "Transaction 1 completed! First byte: %02X", data[0]);
    d = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "trans[1] result: %02X %02X %02X %02X %02X %02X %02X %02X",
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

    // transaction 2
    if (err20 != ESP_OK) {
        ESP_LOGI(TAG, "Failed queuing transaction 2");
        return err20;
    }
    esp_err_t err21 = spi_slave_get_trans_result(spi_host, &out_trans, portMAX_DELAY);
    if (err21 == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Timed out waiting for Master on transaction 2");
        return err21;
    } else if (err21 != ESP_OK) {
        ESP_LOGE(TAG, "SPI Error: %s", esp_err_to_name(err21));
        return err21;
    }
    uint64_t timestamp_trans2 = buf_to_uint64(*(std::array<uint8_t,8>*)out_trans->rx_buffer);
    data = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "Transaction 2 completed! First byte: %02X", data[0]);
    d = (uint8_t*)out_trans->rx_buffer;
    ESP_LOGI(TAG, "trans[2] result: %02X %02X %02X %02X %02X %02X %02X %02X",
        d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);


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

    // if (recv_time_err != ESP_OK) {
    //     ESP_LOGI(TAG, "Error with transmission");
    //     return recv_time_err;
    // }

    for (int i = 0; i < 3; i++) {
        delete trans[i];
    }

    t2.tv_usec = timestamp_trans2 % 1000000L;
    t2.tv_sec  = (int64_t)(timestamp_trans2 / 1000000L);
    t1.tv_usec = timestamp_trans3 % 1000000L;
    t1.tv_sec  = (int64_t)(timestamp_trans3 / 1000000L);

    struct timeval sub1;
    struct timeval sub2;
    struct timeval offset;
    struct timeval cur_time;

    sub_timeval(t1, t0, &sub1);
    sub_timeval(t2, t3, &sub2);
    add_timeval(sub1, sub2, &offset);
    half_timeval(&offset);

    // raw bytes
    uint8_t* rb1 = (uint8_t*)recvbuf[1];
    uint8_t* rb2 = (uint8_t*)recvbuf[2];
    ESP_LOGI(TAG, "recvbuf[1]: %02X %02X %02X %02X %02X %02X %02X %02X",
        rb1[0], rb1[1], rb1[2], rb1[3], rb1[4], rb1[5], rb1[6], rb1[7]);
    ESP_LOGI(TAG, "recvbuf[2]: %02X %02X %02X %02X %02X %02X %02X %02X",
        rb2[0], rb2[1], rb2[2], rb2[3], rb2[4], rb2[5], rb2[6], rb2[7]);


    // parsed timestamps
    ESP_LOGI(TAG, "t0: %lld.%06lld", (int64_t)t0.tv_sec, (int64_t)t0.tv_usec);
    ESP_LOGI(TAG, "t1 (from teensy): %lld.%06lld", (int64_t)t1.tv_sec, (int64_t)t1.tv_usec);
    ESP_LOGI(TAG, "t2 (from teensy): %lld.%06lld", (int64_t)t2.tv_sec, (int64_t)t2.tv_usec);    
    ESP_LOGI(TAG, "t3: %lld.%06lld", (int64_t)t3.tv_sec, (int64_t)t3.tv_usec);

    // intermediate calculations
    ESP_LOGI(TAG, "sub1 (t0-t1): %lld.%06lld", (int64_t)sub1.tv_sec, (int64_t)sub1.tv_usec);
    ESP_LOGI(TAG, "sub2 (t3-t2): %lld.%06lld", (int64_t)sub2.tv_sec, (int64_t)sub2.tv_usec);
    ESP_LOGI(TAG, "offset: %lld.%06lld", (int64_t)offset.tv_sec, (int64_t)offset.tv_usec);
          gettimeofday(&cur_time, NULL);
    // final
    ESP_LOGI(TAG, "cur_time before: %lld.%06lld", (int64_t)cur_time.tv_sec, (int64_t)cur_time.tv_usec);
    add_timeval(cur_time, offset, &cur_time);
    settimeofday(&cur_time, NULL);
    ESP_LOGI(TAG, "cur_time after:  %lld.%06lld", (int64_t)cur_time.tv_sec, (int64_t)cur_time.tv_usec);

  
    
    ESP_LOGI(TAG, "Err: %d", settimeofday(&cur_time, NULL));
    ESP_LOGI(TAG, "Seconds: %lld, Microseconds: %lld", (int64_t)cur_time.tv_sec, (int64_t)cur_time.tv_usec);
    return ESP_OK;
}
