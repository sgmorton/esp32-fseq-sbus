# ESP32 FSEQ → SBUS Bridge

Reads an xLights-generated **FSEQ** file from SD card and outputs **FrSky SBUS** commands on a UART pin. The ESP32 hardware UART inversion handles SBUS logic levels, so no external inverter transistor is required.

## What this repo does

- Parses a single FSEQ sequence from SD.
- Streams **16 SBUS channels** at the FSEQ frame rate.
- Uses standard FrSky SBUS framing: **100000 baud, 8E2, inverted logic**. The code outputs the raw inverted-style frame bytes; you still need a hardware inverter stage to make it electrically compatible with SBUS.
- Designed to be expanded later to playlists, single-file selection, and perhaps OLED status.

## Hardware

- ESP32 board
- MicroSD breakout / reader wired to SPI
- Optional: USB or direct wiring from ESP32 UART pins to SBUS mixer/FC

## Suggested wiring

| ESP32 | SD breakout |
|---|---|
| GPIO18 SCK | SCK |
| GPIO19 MISO | MISO |
| GPIO23 MOSI | MOSI |
| GPIO5 CS | CS |
| 3V3 | 3V3 |
| GND | GND |

### SBUS electrical notes

The ESP32 hardware UART inversion is used for both SBUS input and output.
That means the ESP32 provides the inverted logic directly on the pin, so no external inverter transistor is needed.

## Software

Prereqs: PlatformIO Core.

- Install ESP32 platform: `pio platform install espressif32`
- Build: `pio run`
- Upload: `pio run --target upload`
- Monitor: `pio device monitor`

## Project structure

- `src/main.cpp` top-level sketch
- `src/SBusEncoder.h/.cpp` SBUS frame builder
- `src/SBusParser.h/.cpp` SBUS stream parser for input pass-through
- `src/FSeqPlayer.h/.cpp` minimal FSEQ reader

## Data path

1. Place your FSEQ file into the SD root as `sequence.fseq` or edit `DEFAULT_FSEQ_PATH`.
2. Power up ESP32; it opens the file and begins streaming SBUS frames.
3. Frame interval uses the FSEQ step time; falls back to 50 ms if missing.

## SBUS pass-through input

- SBUS input: `Serial2` on `GPIO16`, 100000/8E2/inverted
- SBUS output: `Serial1` on `GPIO17`, 100000/8E2/inverted
- If valid SBUS frames arrive, **all nonzero channels on the incoming stream override the FSEQ channel of the same index**, so mixer passthrough takes over the whole channel set during active input.

## FrSky SBUS output

Output bytes match a FrSky RX SBUS stream, with hardware-level inversion included by the ESP32 UART.

## Next steps

- Playlist / file selection
- OLED status display
- FRAM/SD buffering pre-roll for show start sync
- Configurable channel mapping and scaling curves
