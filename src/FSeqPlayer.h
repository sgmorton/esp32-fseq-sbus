#pragma once
#include <Arduino.h>
#include "SD.h"

// Minimal FSEQ v1/v2 parser.
// Supports:
//  - v2 header with compression type = none / zlib.
//  - Optional Sparse Range tables.
//  - Reads sequential frames for a given channel subset.
//
// Channel mapping:
//  - fseqChannel = logical channel index inside the file.
//  - We only expose channels 0..NUM_SBUS_CH-1 (16).

class FSeqPlayer {
public:
    static constexpr uint16_t NUM_SBUS_CH = 16;

    struct FileState {
        File file;
        uint32_t channelCount;
        uint32_t frameCount;
        uint32_t stepTimeMs;
        uint32_t channelDataOffset;
        uint8_t  compressionType;
        uint16_t compressionBlockCount;
        uint16_t sparseRangeCount;
        uint8_t* fileBuffer;
        size_t    fileBufferSize;
        size_t    currentFrameOffset; // next frame byte offset within compressed stream
        size_t    uncompressedBlockSize;
        uint8_t*  uncompressedBuffer;
        size_t    uncompressedBufferSize;

        // Sparse info: which fseq channels are mapped.
        // We support only the first NUM_SBUS_CH channels.
        uint32_t mappedFseqChannel[NUM_SBUS_CH];
        uint8_t  mappedChannelCount;
    };

    FSeqPlayer();
    ~FSeqPlayer();

    // Open an FSEQ file. Returns true on success.
    bool open(FileState& state, const char* path);

    // Read one frame's DMX values for the first NUM_SBUS_CH channels.
    // Returns values in 0..255 into outChannels.
    bool readFrame(FileState& state, uint8_t outChannels[NUM_SBUS_CH]);

    void close(FileState& state);

private:
    static bool readHeader(File& file, FileState& state);
    static bool seekToChannelData(File& file, FileState& state);
    static bool ensureUncompressed(FileState& state, const uint8_t* src, size_t srcLen, size_t needed);
    static bool readSparseRanges(File& file, FileState& state);
    static uint16_t crc16(const uint8_t* data, size_t len);
};
