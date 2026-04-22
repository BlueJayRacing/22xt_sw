#include "wsg/wsg_functions.hpp"
#include "util/debug_util.hpp"

namespace baja {
namespace wsg_streaming {

static buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>* fastBuffer_ = nullptr;
static buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>* mainBuffer_ = nullptr;

static SpiWsgRecv data_recv;
static NTPviaSPI sync_handler;

static int total_sample_count = 0;
static bool started_sync = false;

static int count = 0;
static int sync_rate = 10;

static bool override_sync = false;

void init(SPIClass * spi_interface, uint8_t cs_pin, uint8_t handshake_pin, SPISettings settings,
    buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer
) {
    fastBuffer_ = &fastBuffer;
    mainBuffer_ = &mainBuffer;

    spi_interface->setMISO(39);
    baja::util::Debug::info("Starting esp spi interface");
    sync_handler.init(spi_interface, cs_pin, handshake_pin, settings);
    data_recv.init(spi_interface, cs_pin, handshake_pin, settings);
}

void process() {
    std::array<wsg_data_t, MESSAGES_PER_DATA_SEND> msgs;

    if (!started_sync) {
        bool want_to_start_sync = count % sync_rate == 0 && !override_sync;
        if(data_recv.recv(&msgs, want_to_start_sync)) {
            total_sample_count += MESSAGES_PER_DATA_SEND;
            count++;
            started_sync = want_to_start_sync;

            for (int w = 0; w < MESSAGES_PER_DATA_SEND; w++) {
                if (msgs[w].wsg_id > 1) return; // data is corrupted

                uint8_t base_id = msgs[w].wsg_id == 0 ? util::WSG0_BASE_CHANNEL_ID : util::WSG1_BASE_CHANNEL_ID;
                for(int i = 0; i < 3; i++) {
                    data::ChannelSample channelSample(
                        msgs[w].timestamp,
                        base_id + i,
                        msgs[w].sample[i],
                        millis()
                    );

                    // add channel to buffers
                    mainBuffer_->write(channelSample);
                    fastBuffer_->write(channelSample);
                }
            }
            // // add stuff to channel
            // for (wsg_data_t wsg : msgs) {
            //     util::Debug::info(F("id: ") + String(wsg.wsg_id));
            //     util::Debug::info(F("Timestamp: ") + String(wsg.timestamp));
            //     util::Debug::info(F("Dac Bias: ") + String(wsg.dac_bias));
            //     util::Debug::info(F("Sample 1: ") + String(wsg.sample[0]));
            //     util::Debug::info(F("Sample 2: ") + String(wsg.sample[1]));
            //     util::Debug::info(F("Sample 3: ") + String(wsg.sample[2]));
            // }
        }
    } else {
        sync_handler.sync();
        baja::util::Debug::info("Time synced with esp");
        started_sync = false;
    }
}

}
}