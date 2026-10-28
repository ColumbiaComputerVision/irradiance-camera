# An Eternal Irradiance Camera

This repository contains the hardware design files, firmware, 3D models, and
reconstruction code for the SIGGRAPH Asia 2026 paper "An Eternal Irradiance
Camera."

#### [[Paper](https://cave.cs.columbia.edu/Statics/publications/pdfs/Klotz_SIGGRAPHAsia26.pdf)] [[Project Page](https://cave.cs.columbia.edu/projects/categories/project?cid=Computational+Imaging&pid=An+Eternal+Irradiance+Camera)] [[Video](https://www.youtube.com/watch?v=OySHER_N6bE)] [[Supplemental](https://www.cs.columbia.edu/~jklotz/papers/irradiance-cam/irradiance-cam-supp.pdf)]
[Jeremy Klotz](https://www.cs.columbia.edu/~jklotz/) and [Shree K. Nayar](https://www.cs.columbia.edu/~nayar/)

## Circuit Schematics and Layouts

[`hardware/`](hardware/README.md) contains the Altium projects and PDF
schematics/layouts for the sensor boards and processor boards.  See also: [board changes](hardware/README.md#board-changes).

## Firmware

[`firmware/`](firmware/README.md) contains the firmware for the processor
board's STM32L051 microcontroller, CC1310 RF transmitter, and the
LAUNCHXL-CC1310 LaunchPad used as the RF receiver. See
[`firmware/README.md`](firmware/README.md) for the frame format and build
instructions.

## 3D Models

[`3d_models/`](3d_models) contains the 3D models for the camera's spherical shell. The shell was printed in four quadrants using a Formlabs Form 4 SLA printer.

## Reconstruction

[`software/reconstruction.py`](software/reconstruction.py) reconstructs the
irradiance function from the 49 measurements made by the prototype. This example code uses real data captured by the prototype camera.

Requirements: Python 3.13, `numpy`, `scipy` (>= 1.15), `healpy`, `matplotlib`,
`qpsolvers[quadprog]`.

To set up the conda environment, run:
```bash
conda env create -f software/environment.yml
conda activate irradiance-cam
```

Then run the reconstruction code:

```bash
python software/reconstruction.py
python software/reconstruction.py --data software/data/capture-2.npz # reconstruct the irradiance function in another scene
```

Each capture in `software/data` holds 2 seconds of raw sensor data (at 30 fps) from all 49 detectors, in lux. The script loads it, plots the raw measurements, averages each detector's measurement over the 2 seconds, and fits the
irradiance function to those averages.

In our prototype, detectors 1 and 37 had hardware issues. So we did not use their measurements in the reconstruction (these detectors are `False` in `sensor_mask`). We also exclude any detector whose measurements are saturated from participating in the reconstruction.

## Citation

```bibtex
@InProceedings{klotzIrradiance2026,
  author  = {Jeremy Klotz and Shree K. Nayar},
  title   = {An Eternal Irradiance Camera},
  booktitle = {Proceedings of SIGGRAPH Asia},
  month   = {December},
  year    = {2026}
}
```
