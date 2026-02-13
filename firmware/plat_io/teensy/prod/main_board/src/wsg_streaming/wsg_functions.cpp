#include "wsg_streaming/wsg_functions.hpp"
#include "util/debug_util.hpp"

namespace baja {
namespace wsg_streaming {
namespace functions {


static SPISettings esp_spi_settings(10000000, MSBFIRST, SPI_MODE0);
static SpiWsgRecv receiver(&SPI1, 36, esp_spi_settings);
static buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>* fastBuffer_ = nullptr;
static buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>* mainBuffer_ = nullptr;
static uint16_t channelSampleCounters_[util::TOTAL_CHANNEL_COUNT] = {0};
static uint8_t sample_ct[8] = {0};

bool initialize(
        buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
        buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer
) {
    fastBuffer_ = fastBuffer;
    mainBuffer_ = mainBuffer;
}

void process() {
    std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> arr;

    wsg_data.recv(arr);

    for (int i = 0; i < arr.length(); i++) {
        uint8_t channel_id = arr[i].wsg_id << 2 | i;
        data::ChannelSample channelSample(
            arr[i].timestamp,
            17 << 5 | channel_id,
            arr[i].sample,
            millis()
        );

        if (fastBuffer_) {
            // change to better channel desc
            sample_ct[channel_id]++;
            if(sample_ct[channel_id] >= config::FAST_BUFFER_DOWNSAMPLE_RATIO) {
                fastBuffer_->write(channelSample);
                sample_ct[channel_id] = 0;
            }
        }
    }
}

}
}
}