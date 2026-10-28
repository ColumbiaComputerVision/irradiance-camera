# Hardware

Schematics and PCB layouts for the irradiance camera.
The corresponding firmware is in [`../firmware`](../firmware/README.md).

> **The design files specify the boards that we fabricated.** We performed the additional hardware modifications described below.

## Boards

| Board | Altium project | Schematic and layout | Description |
|-------|----------------|----------------------|-------------|
| Sensor board | [`sensor-board-altium-project.zip`](sensor-board-altium-project.zip) | [`Sensor Board.pdf`](<Sensor Board.pdf>) | One per pixel; the camera uses 49. A solar cell (`J2`) is both the light sensor and the energy source. An ADG787 switch (`U1`) connects the cell either to a bq25505 energy harvester (`U5`) or to a transimpedance amplifier (`U2`, OPA607 after rework) and an AD7683 16-bit ADC (`U3`). Connects to the processor board through `J1` (Samtec FTS-104-01-L-DV). |
| Processor board | [`processor-board-altium-project.zip`](processor-board-altium-project.zip) | [`Processor Board.pdf`](<Processor Board.pdf>) | STM32L051C6T (`U5`) that sequences the sensor readout and sleeps between frames, and a CC1310 (`U6`) sub-GHz radio that transmits each frame. Also has 49 sensor board connectors, the analog muxes that route TIA enable and ADC chip select to each sensor board (`U1`–`U3`, `U7`–`U9` ADG706; `U10`, `U11` TMUX1101), the supercapacitor (`C1`) that stores harvested energy from all sensor boards, and a 3.3 V step-down converter (`U4`, XCL210). Programming connectors: `J3` (STM32, SWD) and `J5` (CC1310, JTAG). |
| RF receiver | — | — | Off-the-shelf TI **LAUNCHXL-CC1310** LaunchPad. No custom hardware; see [`../firmware`](../firmware/README.md). |

## Files

- **`*-altium-project.zip`:** the Altium project for each board. Unzip it and
  open the `.PrjPcb` file in Altium Designer. Each contains the project
  (`.PrjPcb`), schematic sheets (`.SchDoc`), and PCB layout (`.PcbDoc`). The
  sensor board project also includes its Output Job (`.OutJob`), BOM
  (`.BomDoc`), and assembly variants (`.PrjPcbVariants`).
- **`* Board.pdf`:** every schematic sheet, followed by the PCB layout, for
  viewing without Altium.

## Board changes

The Altium projects in this folder are the boards **as fabricated**. The boards
used in the paper were modified after fabrication as listed below. If you build
these boards, apply the same changes.

Changes were made to the **sensor board** and the **processor board**.

### Sensor board

| # | Change | Designator(s) | Original | As built |
|---|--------|---------------|----------|----------|
| 1 | Remove the pre-ADC RC filter | `R2`, `C5` | `R2` = 100 Ω in series between the op amp output and ADC `+IN`, `C5` = 220 pF from ADC `+IN` to GND | `R2` jumpered, `C5` removed |
| 2 | Change the op amp | `U2` | MAX9911EXT | OPA607 |
| 3 | Change the MPPT divider resistor | `R3` | 4.12 MΩ | 2 MΩ |
| 4 | Add a pull-up on the ADC chip select | (not on PCB) | none | 100 kΩ from `U3` CS (net `SCS`, `J1` pin 5) to `VCC` |
| 5 | Change the TIA gain | `R1` | 100 kΩ | 4.7 kΩ |

### Processor board

| # | Change | Designator(s) | Original | As built |
|---|--------|---------------|----------|----------|
| 1 | Add a second supercapacitor in parallel with the existing one | Added (not on PCB), across `C1` | `C1` = 47 mF, 5.5 V on `HARV_IN` | `C1` + KYOCERA AVX BestCap BZ015B603ZSB (60 mF, 5.5 V) |

### Cables

We used Samtec FFSD-04 ribbon cables to connect each sensor board to the processor board.
