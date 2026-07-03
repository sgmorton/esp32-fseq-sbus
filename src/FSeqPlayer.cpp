#include "FSeqPlayer.h"
#include <string.h>

// zlib for zlib-compressed FSEQ v2.
// If compressed FSEQ files are used, add "zlib" to platformio lib_deps.
#if __has_include(<zlib.h>)
#include <zlib.h>
#define HAS_ZLIB 1
#else
#define HAS_ZLIB 0
#endif

FSeqPlayer::FSeqPlayer() = default;
FSeqPlayer::~FSeqPlayer() = default;

// CRC-16/CCITT-FALSE over a byte buffer.
uint16_t FSeqPlayer::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc <<= 1;
        }
    }
    return crc;
}

bool FSeqPlayer::readHeader(File& file, FileState& state) {
    uint8_t hdr[32];
    if (file.read(hdr, 32) != 32) return false;

    if (memcmp(hdr, "PSEQ", 4) != 0 && memcmp(hdr, "FSEQ", 4) != 0) return false;

    state.channelDataOffset = ((uint16_t)hdr[4]) | ((uint16_t)hdr[5] << 8);
    state.compressionType   = (hdr[20] >> 4) & 0x0F;
    state.compressionBlockCount = ((uint16_t)(hdr[20] & 0x0F) << 8) | hdr[21];
    state.sparseRangeCount  = hdr[22];
    state.channelCount      = ((uint32_t)hdr[10]) | ((uint32_t)hdr[11] << 8) |
                             ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
    state.frameCount        = ((uint32_t)hdr[14]) | ((uint32_t)hdr[15] << 8) |
                             ((uint32_t)hdr[16] << 16) | ((uint32_t)hdr[17] << 24);
    state.stepTimeMs        = hdr[18];

    return true;
}

bool FSeqPlayer::readSparseRanges(File& file, FileState& state) {
    state.mappedChannelCount = 0;
    if (state.sparseRangeCount == 0) {
        // All channels are present in order.
        for (uint16_t i = 0; i < state.channelCount && state.mappedChannelCount < NUM_SBUS_CH; i++) {
            state.mappedFseqChannel[state.mappedChannelCount++] = i;
        }
        return true;
    }

    // Each sparse range entry is 12 bytes:
    // uint32 startChannel
    // uint32 channelCount
    // uint32 reserved
    for (uint16_t s = 0; s < state.sparseRangeCount; s++) {
        uint8_t buf[12];
        if (file.read(buf, 12) != 12) return false;

        uint32_t startCh = ((uint32_t)buf[0]) | ((uint32_t)buf[1] << 8) |
                           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
        uint32_t countCh = ((uint32_t)buf[4]) | ((uint32_t)buf[5] << 8) |
                           ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);

        // Pick first NUM_SBUS_CH channels we care about.
        for (uint32_t i = 0; i < countCh && state.mappedChannelCount < NUM_SBUS_CH; i++) {
            state.mappedFseqChannel[state.mappedChannelCount++] = startCh + i;
        }
    }

    if (state.mappedChannelCount == 0) return false;
    return true;
}

bool FSeqPlayer::open(FileState& state, const char* path) {
    memset(&state, 0, sizeof(FileState));
    state.file = SD.open(path, FILE_READ);
    if (!state.file) return false;

    if (!readHeader(state.file, state)) {
        state.file.close();
        return false;
    }
    if (!readSparseRanges(state.file, state)) {
        state.file.close();
        return false;
    }
    if (!seekToChannelData(state.file, state)) {
        state.file.close();
        return false;
    }

    state.currentFrameOffset = 0;
    state.fileBufferSize     = 4096;
    state.fileBuffer         = (uint8_t*)ps_malloc(state.fileBufferSize);
    state.uncompressedBufferSize = (state.channelCount > NUM_SBUS_CH ? NUM_SBUS_CH : state.channelCount) + 64;
    state.uncompressedBuffer = (uint8_t*)ps_malloc(state.uncompressedBufferSize);
    if (!state.fileBuffer || !state.uncompressedBuffer) {
        close(state);
        return false;
    }
    return true;
}

bool FSeqPlayer::seekToChannelData(File& file, FileState& state) {
    // Skip any remaining header bytes after fixed/sparse tables.
    uint32_t pos = 32;
    if (state.compressionBlockCount > 0) {
        pos += (uint32_t)state.compressionBlockCount * 8;
    }
    if (state.sparseRangeCount > 0) {
        pos += (uint32_t)state.sparseRangeCount * 12;
    }
    // Small tolerance for alignment padding.
    if (pos < state.channelDataOffset) {
        file.seek(state.channelDataOffset);
    } else {
        file.seek(pos);
    }
    return true;
}

#if HAS_ZLIB
bool FSeqPlayer::ensureUncompressed(FileState& state, const uint8_t* src, size_t srcLen, size_t needed) {
    if (state.uncompressedBufferSize < needed) {
        uint8_t* nb = (uint8_t*)realloc(state.uncompressedBuffer, needed);
        if (!nb) return false;
        state.uncompressedBuffer = nb;
        state.uncompressedBufferSize = needed;
    }
    uLongf destLen = (uLongf)needed;
    if (uncompress(state.uncompressedBuffer, &destLen, src, srcLen) != Z_OK) {
        return false;
    }
    return true;
}
#else
bool FSeqPlayer::ensureUncompressed(FileState&, const uint8_t*, size_t, size_t) {
    return false;
}
#endif

bool FSeqPlayer::readFrame(FileState& state, uint8_t outChannels[NUM_SBUS_CH]) {
    if (!state.file) return false;
    if (state.currentFrameOffset >= state.frameCount) return false;

    if (state.compressionType == 0) {
        // Uncompressed: each frame is channelCount bytes back-to-back.
        for (uint8_t i = 0; i < NUM_SBUS_CH; i++) {
            uint32_t chIdx = state.mappedFseqChannel[i];
            if (!state.file.seek(state.channelDataOffset + (state.currentFrameOffset * state.channelCount) + chIdx)) {
                return false;
            }
            outChannels[i] = state.file.read();
        }
    } else if (state.compressionType == 1 || state.compressionType == 2) {
#if HAS_ZLIB
        // We simplify here: decompress a whole block sized for one frame of mapped channels.
        // For real sequences, this should read the compression-block table and reuse it.
        size_t needed = (state.channelCount > NUM_SBUS_CH ? NUM_SBUS_CH : state.channelCount);
        size_t blockLen = (needed * 3 / 2) + 64; // rough estimate for LZ-like sources
        uint8_t* compBuf = (uint8_t*)ps_malloc(blockLen);
        if (!compBuf) return false;
        size_t got = state.file.read(compBuf, blockLen);
        if (got == 0 || !ensureUncompressed(state, compBuf, got, needed)) {
            free(compBuf);
            return false;
        }
        free(compBuf);
        for (uint8_t i = 0; i < NUM_SBUS_CH; i++) {
            uint32_t chIdx = state.mappedFseqChannel[i];
            if (chIdx < needed) outChannels[i] = state.uncompressedBuffer[chIdx];
            else outChannels[i] = 0;
        }
#else
        (void)state; (void)outChannels;
        return false;
#endif
    } else {
        return false;
    }

    state.currentFrameOffset++;
    return true;
}

void FSeqPlayer::close(FileState& state) {
    if (state.file) state.file.close();
    if (state.fileBuffer)          free(state.fileBuffer);
    if (state.uncompressedBuffer)  free(state.uncompressedBuffer);
    memset(&state, 0, sizeof(FileState));
}
