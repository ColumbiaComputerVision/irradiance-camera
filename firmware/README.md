# Firmware

Firmware for the three microcontrollers in the camera system. Hardware design
files are in [`../hardware`](../hardware/README.md).

| Folder | MCU | Runs on | Toolchain |
|--------|-----|---------|-----------|
| [`processor_board_firmware`](processor_board_firmware) | STM32L051C6T | Processor board | STM32CubeIDE (CubeMX 6.17.0, STM32Cube FW_L0 V1.12.4, LL drivers) |
| [`CC1310-rf-transmitter`](CC1310-rf-transmitter) | CC1310F128 | Processor board | Code Composer Studio, SimpleLink CC13x0 SDK 4.20.02.07 (TI-RTOS) |
| [`rf-receiver`](rf-receiver) | CC1310F128 | TI LAUNCHXL-CC1310 LaunchPad | Code Composer Studio, SimpleLink CC13x0 SDK 4.20.02.07 (no RTOS) |

The RF receiver is an off-the-shelf **LAUNCHXL-CC1310** development kit. Connect it to a PC over USB to receive data.

The `README.md` files inside the two CC1310 project folders are TI's original
EasyLink example READMEs and do not describe this firmware.

## How it works

1. **Frame timer.** The STM32 sleeps in STOP mode and is woken by LPTIM1
   (clocked from the ~37 kHz LSI) at `FPS` frames per second (default 30).
2. **Sensor readout.** For each of the 49 sensor boards, the STM32 sets the
   mux address (`CTR_A0`–`CTR_A7`). Two sets of analog muxes on the processor
   board then route the TIA enable (`TIA_ENBL`) and the ADC chip select
   (`SPI1_CS`) to that sensor board. The STM32 waits a settling time (TIM6,
   `SETTLING_TIME_PERIOD`), then reads 3 bytes over SPI1 from the sensor
   board's AD7683 16-bit ADC. It shifts the 24-bit word right by 2 and stores
   the result as a 16-bit reading.
3. **Hand-off to the radio.** The STM32 raises `RF_WAKEUP`. The CC1310 wakes,
   queues an SPI transfer, and raises `SPI_READY`. The STM32 then sends
   one 98-byte frame (49 × `uint16`, little-endian) over SPI2.
4. **Transmit.** The CC1310 prepends a 16-bit little-endian sequence number and
   sends the 100-byte payload with EasyLink to address `0xAA` (868 MHz,
   200 kbps, 0 dBm).
5. **Receive.** The LaunchPad receives each packet and writes it to its USB
   serial port (XDS110 back-channel UART, 460800 baud, 8N1) as a 108-byte
   frame:

   | Bytes | Content |
   |-------|---------|
   | 0–7 | Header, 8 × `0xEE` |
   | 8–9 | Sequence number, `uint16` little-endian |
   | 10–107 | 49 sensor readings, `uint16` little-endian |

### STM32 ↔ CC1310 connections

| Signal | STM32 pin | CC1310 pin |
|--------|-----------|------------|
| `RF_WAKEUP` (STM32 → CC1310) | PB11 | DIO1 |
| `SPI_READY` (CC1310 → STM32) | PB9 | DIO3 |
| SPI2 CS | PB12 | DIO11 |
| SPI2 SCK | PB13 | DIO10 |
| SPI2 MOSI | PB15 | DIO9 |
| `RF_RESET` (STM32 holds the CC1310 in reset until boot) | PB1 | RESET |

## Building and flashing

### Processor board (STM32)

1. In STM32CubeIDE, choose **File → Import → Existing Projects into
   Workspace** and select `processor_board_firmware`.
2. Build the **Release** or **Debug** configuration.
3. Flash with an ST-LINK through `J3` on the processor board (SWD, 6-pin JST
   SH).

The pin and peripheral setup lives in `processor_board_firmware.ioc`. If you
regenerate code with CubeMX, only edit `main.c` inside the `USER CODE` blocks.

### CC1310 transmitter and receiver

1. Install Code Composer Studio and the **SimpleLink CC13x0 SDK 4.20.02.07**.
   Other SDK versions may need changes.
2. In CCS, choose **Project → Import CCS Projects** and select the project
   folder.
   - The transmitter (TI-RTOS) depends on the SDK's TI-RTOS kernel project,
     `tirtos_builds_CC1310_LAUNCHXL_release_ccs`. Import it into the same
     workspace from
     `<SDK_INSTALL_DIR>/kernel/tirtos/builds/CC1310_LAUNCHXL/release/ccs`. The
     receiver (no RTOS) does not need it.
3. Build and flash with an XDS110 debug probe. The receiver LaunchPad has one
   on board. The transmitter is programmed through `J5` on the processor board
   (JTAG, 7-pin JST SH).

## Build options

Processor board, at the top of `processor_board_firmware/Core/Src/main.c`:

| Define | Default | Effect |
|--------|---------|--------|
| `FPS` | 30 | Frame rate |
| `SETTLING_TIME_PERIOD` | 450 | TIA settling time per sensor, in TIM6 ticks. Longer settling uses more power; the comments next to it list measured currents. Set to 0 to skip. |
| `ENABLE_RF` | defined | Boot the CC1310 and send each frame to it. Comment out to run the STM32 alone. |
| `RX_TESTER` | not defined | Replace sensor readings with a counter and a fixed ramp, for testing the radio link. |

CC1310 transmitter: `DEVKIT` (run on a LaunchPad instead of the custom board)
and `USE_LEDS` (blink LEDs on transmit). Add these to the project's predefined
symbols (**Project Properties → Build → ARM Compiler → Predefined Symbols**),
not as a `#define` in `rfEasyLinkTx.c`. They are checked in more than one
source file.
