#include <Arduino.h>
#include <esp_heap_caps.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SBusEncoder.h"
#include "FSeqPlayer.h"

// ---- User-configurable pins ----
static constexpr uint8_t PIN_SD_CS = 5;
static constexpr uint8_t PIN_SD_MOSI = 23;
static constexpr uint8_t PIN_SD_MISO = 19;
static constexpr uint8_t PIN_SD_SCK = 18;
static constexpr uint8_t PIN_SBUS_TX = 17;   // Use with hardware inverter

// ---- Runtime options ----
static constexpr uint32_t DEFAULT_STEP_TIME_MS = 50;  // override if FSEQ differs
static const char* DEFAULT_FSEQ_PATH = "/sequence.fseq";

SPIClass spiSD(VSPI);
FSeqPlayer::FileState fseq{};
SBusEncoder sbus;
uint8_t sbusBuffer[25];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  delay(300);

  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS, spiSD)) {
    Serial.println("SD init failed.");
    while (true) delay(1000);
  }

  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());

  if (!fseq.open(fseq, DEFAULT_FSEQ_PATH)) {
    Serial.printf("Failed to open '%s'\n", DEFAULT_FSEQ_PATH);
    while (true) delay(1000);
  }

  Serial.printf("FSEQ loaded: channels=%lu frames=%lu step=%lums\n",
                (unsigned long)fseq.channelCount,
                (unsigned long)fseq.frameCount,
                (unsigned long)fseq.stepTimeMs);

  // SBUS UART on selected pin, 100000/8E2/inverted.
  // For ESP32 hardware serial, you can use Serial1/SERIAL_1 etc.
  // Many users route this through GPIO expanders or UART -> inverter transistor.
  Serial1.begin(100000, SERIAL_8E2, -1, PIN_SBUS_TX, true);

  // Send 5 startup frames as elevon/center values.
  SBusEncoder::Frame starter{};
  for (int i = 0; i < 16; i++) starter.channels[i] = 1500;
  starter.flags = 0x00;
  sbus.encodeFrame(starter, sbusBuffer);
  for (int i = 0; i < 5; i++) Serial1.write(sbusBuffer, 25);
}

void loop() {
  uint8_t channels[FSeqPlayer::NUM_SBUS_CH] = {0};

  if (!fseq.readFrame(fseq, channels)) {
    // End of show: loop back to start.
    fseq.file.seek(fseq.channelDataOffset);
    fseq.currentFrameOffset = 0;
    if (!fseq.readFrame(fseq, channels)) {
      delay(1000);
      return;
    }
  }

  // Map 0..255 -> FrSky ppm range roughly 988..2011.
  SBusEncoder::Frame frame{};
  for (uint8_t i = 0; i < FSeqPlayer::NUM_SBUS_CH; i++) {
    // Scale 8-bit animation to ppm range centered around ~1500us.
    uint16_t ppm = 988 + (uint32_t)channels[i] * (2011 - 988) / 255;
    frame.channels[i] = ppm;
  }
  frame.flags = 0x00;

  sbus.encodeFrame(frame, sbusBuffer);
  Serial1.write(sbusBuffer, 25);

  if (fseq.stepTimeMs) {
    delay(fseq.stepTimeMs);
  } else {
    delay(DEFAULT_STEP_TIME_MS);
  }
}
