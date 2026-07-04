# ESP32 FSEQ → SBUS Bridge

Reads an xLights-generated **FSEQ** file from SD card and outputs **FrSky SBUS** commands on a UART pin, intended for use with a hardware inverter into a typical SBUS-capable mixer/receiver input.

## What this repo does

- Parses a single FSEQ sequence from SD.
- Streams **16 SBUS channels** at the FSEQ frame rate.
- Uses standard FrSky SBUS framing: **100000 baud, 8E2, inverted logic**. The code outputs the raw inverted-style frame bytes; you still need a hardware inverter stage to make it electrically compatible with SBUS.
- Designed to be expanded later to playlists, single-file selection, and perhaps OLED status.

## Hardware

- ESP32 board
- MicroSD breakout / reader wired to SPI
- SBUS transmitter path: **ESP32 UART TX -> inverter transistor -> SBUS wire**

## Suggested wiring

| ESP32 | SD breakout |
|---|---|
| GPIO18 SCK | SCK |
| GPIO19 MISO | MISO |
| GPIO23 MOSI | MOSI |
| GPIO5 CS | CS |
| 3V3 | 3V3 |
| GND | GND |

### SBUS inverter

A common inverter:

| Component | Notes |
|---|---|
| NPN/PNP + pullup | Inverts UART idle/space levels |
| One inverter IC | e.g. 74HC14D stage |
| Level shifter | Optional, only if running 5V SBUS devices |

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

Raw bytes match a FrSky RX SBUS output stream; still apply an hardware stage so idle = MARK and data = SPACE at SBUS levels. Typical FrSky receivers/FCs expect that inversion.

## Next steps

- Playlist / file selection
- OLED status display
- FRAM/SD buffering pre-roll for show start sync
- Configurable channel mapping and scaling curves
