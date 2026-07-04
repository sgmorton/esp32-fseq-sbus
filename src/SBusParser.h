#pragma once
#include <Arduino.h>

// Parser for FrSky SBUS input stream (100000 8E2 inverted).
// Use alongside a UART configured with invert=true.

class SBusParser {
public:
    static constexpr size_t FRAME_SIZE = 25;
    static constexpr uint8_t START_BYTE = 0x0F;
    static constexpr uint8_t END_BYTE   = 0x00;
    static constexpr uint32_t VALID_TIMEOUT_MS = 150;

    struct Frame {
        uint16_t channels[16];
        uint8_t  flags;
        bool     valid;
    };

    SBusParser();

    // Feed one incoming byte. Returns true when a full valid frame is parsed.
    bool update(uint8_t byte);

    // Most recently parsed frame.
    const Frame& getFrame() const { return lastFrame_; }

    // True if last frame arrived recently enough to be considered active.
    bool isFresh(uint32_t now) const { return valid_ && ((now - lastFrameMs_) < VALID_TIMEOUT_MS); }

    void reset();

private:
    Frame lastFrame_{};
    uint32_t lastFrameMs_{0};
    bool valid_{false};
    uint8_t rxBuffer_[FRAME_SIZE]{};
    uint8_t rxIndex_{0};
};
