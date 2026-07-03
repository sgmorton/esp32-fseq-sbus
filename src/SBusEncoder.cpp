#include "SBusEncoder.h"

SBusEncoder::SBusEncoder() = default;

uint16_t SBusEncoder::usToSbus(uint16_t us) {
    // Map 988-2011 us to 0-2047 SBUS scaled value.
    // FrSky SBUS channel is 11-bit: 0..2047.
    if (us < 988) us = 988;
    if (us > 2011) us = 2011;
    return (uint16_t)((us - 988) * 2047UL / (2011 - 988));
}

void SBusEncoder::encodeFrame(const Frame& frame, uint8_t* buffer) const {
    memset(buffer, 0, 25);
    buffer[0] = START_BYTE;

    // Pack 11-bit channel values into bytes 1-22.
    for (uint8_t ch = 0; ch < 16; ch++) {
        uint16_t value = usToSbus(frame.channels[ch]);
        uint16_t bitPos = ch * 11;
        for (uint8_t bit = 0; bit < 11; bit++) {
            uint16_t byteIdx = 1 + (bitPos / 8);
            uint8_t  bitIdx = bitPos % 8;
            if (value & (1 << bit)) buffer[byteIdx] |= (1 << bitIdx);
            bitPos++;
        }
    }

    buffer[23] = frame.flags;
    buffer[24] = END_BYTE;
}
