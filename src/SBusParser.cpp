#include "SBusParser.h"
#include <string.h>

SBusParser::SBusParser() = default;

void SBusParser::reset() {
    memset(&lastFrame_, 0, sizeof(Frame));
    lastFrameMs_ = 0;
    valid_ = false;
    rxIndex_ = 0;
}

bool SBusParser::update(uint8_t byte) {
    uint32_t now = millis();

    // Restart on start byte instead of assuming fixed frame alignment.
    if (rxIndex_ == 0 && byte != START_BYTE) return false;

    rxBuffer_[rxIndex_++] = byte;

    if (rxIndex_ < FRAME_SIZE) return false;

    // Validate full frame.
    if (rxBuffer_[0] != START_BYTE || rxBuffer_[24] != END_BYTE) {
        rxIndex_ = 0;
        return false;
    }

    Frame frame{};
    for (uint8_t ch = 0; ch < 16; ch++) {
        uint16_t value = 0;
        uint16_t bitPos = ch * 11;
        for (uint8_t bit = 0; bit < 11; bit++) {
            uint16_t byteIdx = 1 + (bitPos / 8);
            uint8_t  bitIdx = bitPos % 8;
            if (rxBuffer_[byteIdx] & (1 << bitIdx)) value |= (1 << bit);
            bitPos++;
        }
        frame.channels[ch] = value;
    }
    frame.flags = rxBuffer_[23];
    frame.valid = true;

    lastFrame_ = frame;
    lastFrameMs_ = now;
    valid_ = true;

    rxIndex_ = 0;
    return true;
}
