# RP2040 to PCM5102A connection guide

This guide shows a simple way to connect a Raspberry Pi Pico / RP2040 board to a PCM5102A DAC module.

## Recommended wiring

The firmware in this repository expects the following default mapping:

- BCK (bit clock) -> GPIO 18
- LCK / WS (word select / left-right clock) -> GPIO 19
- DIN (data input) -> GPIO 20

## Power connections

Connect the PCM5102A module power pins as follows:

- PCM5102A VCC -> RP2040 3.3V
- PCM5102A GND -> RP2040 GND
- PCM5102A VIN / 5V (if required by your module) -> 5V only if the module explicitly requires it

> Many PCM5102A breakout boards are 3.3V compatible for digital I/O and can be powered from 3.3V. If your module has a separate 5V input pin, follow its datasheet.

## Audio pins

| PCM5102A pin | RP2040 GPIO |
| --- | --- |
| BCK | 18 |
| LCK | 19 |
| DIN | 20 |
| GND | GND |
| VCC | 3.3V |

## Optional analog output

Connect the PCM5102A analog outputs to an amplifier or headphones as needed:

- OUTL -> amplifier / input stage
- OUTR -> amplifier / input stage
- AGND -> analog ground

## Firmware note

If your board uses a different GPIO mapping, change the values in [src/pcm5102a_output.c](src/pcm5102a_output.c):

```c
#define PCM5102A_GPIO_BCLK 18
#define PCM5102A_GPIO_LRCLK 19
#define PCM5102A_GPIO_DIN 20
```

## Build note

This workspace currently needs an RP2040 toolchain to build successfully. On a machine with the Pico SDK installed, run:

```bash
cd /workspaces/rp2040-dac
cmake -S . -B build -DPICO_SDK_FETCH_FROM_GIT=ON
cmake --build build
```

## Troubleshooting

- If you hear nothing, check that the PCM5102A is powered correctly and that the BCK/LCK/DIN connections match the firmware.
- If the output is distorted, verify that the GPIO mapping matches your module exactly.
- If the board is not detected over USB, confirm that the TinyUSB audio interface is still enabled and your host is accepting the device.
