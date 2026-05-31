#include "recv_wsg_data.hpp"
#include <spi_ntp.hpp>
#include "util/ring_buffer.hpp"
#include "util/circular_buffer.hpp"
#include "util/sample_data.hpp"
#include "config/config.hpp"
#include <array>

namespace baja {
namespace wsg_streaming {

void init(SPIClass * spi_interface, uint8_t cs_pin, uint8_t handshake_pin, SPISettings settings,
    buffer::RingBuffer<data::ChannelSample, config::SAMPLE_RING_BUFFER_SIZE>& mainBuffer,
    buffer::CircularBuffer<data::ChannelSample, config::FAST_BUFFER_SIZE>& fastBuffer
);

void process();

}
}
