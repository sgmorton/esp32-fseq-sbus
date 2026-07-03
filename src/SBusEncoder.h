#pragma once
#include <Arduino.h>

// Matches FrSky SBUS output as seen by a SBUS-capable receiver/FC.
// Logic levels: 0V = MARK/idle, 5V-ish = SPACE (inverted).
// Hardware: ESP32 UART TX -> inverter -> SBUS line.
//
// FrSky SBUS frame structure (25 bytes):
//  [0]   Start byte = 0x0F
//  [1..22] Channels 1-16 packed across 22 bytes
//      ch1:  bits 0-10 of byte1/byte2
//      ch2:  bits 11-21 of byte2/byte3
//      ... etc
//  [23]  Flags byte
//  [24]  End byte = 0x00
//
// Timing: 100000 baud, 8E2.
// Frame repetition: ~14ms on FrSky RX.

class SBusEncoder {
public:
    static constexpr uint32_t BAUD = 100000;
    static constexpr uint8_t START_BYTE = 0x0F;
    static constexpr uint8_t END_BYTE   = 0x00;

    struct Frame {
        uint16_t channels[16];
        uint8_t flags;
    };

    SBusEncoder();

    // Encode 16 channels + flags into 25-byte buffer.
    // Buffer must be at least 25 bytes.
    void encodeFrame(const Frame& frame, uint8_t* buffer) const;

    // Convert 16-bit channel values into the packed SBUS frame.
    // Values are expected in 988-2011 range (standard FrSky ppm).
    static uint16_t usToSbus(uint16_t us);

private:
    static void setBit(uint8_t* buffer, uint16_t bitIndex, uint16_t value, uint16_t bitCount);
};
