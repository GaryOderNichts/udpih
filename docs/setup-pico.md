# Pico
## Instructions
- Download the latest `udpih_pico.uf2` for the Pico, or `udpih_pico2.uf2` for the Pico 2, from the [releases page](https://github.com/GaryOderNichts/udpih/releases).
- Hold down the `BOOTSEL` button on the board and connect the Pico to your PC.  
  Your PC will detect the Pi as a storage device.
- Copy the `.uf2` file to the Pico. It will disconnect after a few seconds.

The Pico is now flashed and can be used for udpih. Continue with ["Booting the recovery_menu"](../README.md#booting-the-recovery_menu).

## Building from source
1. Set up the [pico-sdk](https://github.com/raspberrypi/pico-sdk).
1. Download the latest `arm_kernel.bin.h` from the [releases page](https://github.com/GaryOderNichts/udpih/releases) and copy it to the `arm_kernel` directory (or build it from source).  
  You can also simply run:
  ```bash
  curl -L https://github.com/GaryOderNichts/udpih/releases/latest/download/arm_kernel.bin.h > arm_kernel/arm_kernel.bin.h
  ```
1. Run `make pico`. The built files will be in the `pico/build_*` directories.
