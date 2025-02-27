# ESP32
## Instructions
- Download the latest `udpih_esp32-s2.zip` for the ESP32-S2, or `udpih_esp32-s3.zip` for the ESP32-S3, from the [releases page](https://github.com/GaryOderNichts/udpih/releases).  
  Other chips don't support USB-OTG or the esp32sx driver, and are not supported.
- Extract the `.bin` files to a folder on your PC. Then open a terminal session (or command prompt in Windows) and navigate to that folder.
- To install UDPIH on your board, you need to have esptool installed. You can download the esptool tool from the [github repo](https://github.com/espressif/esptool) or by running `pip install esptool`.
- Hold down the `BOOT` button on the board and plug it into your PC.

You can now run the following command to flash UDPIH to the board:

> [!NOTE]  
> The `-p xxx` argument needs to be adjusted, depending on your operation system.  
> For Linux this will be something like `/dev/ttyACM0`.
> For macOS `/dev/cu.usbmodem111101`.
> For Windows use the correct COMxxx port.

### ESP32-S2
```
esptool --chip esp32s2 -p xxx --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 bootloader.bin 0x10000 udpih.bin 0x8000 partition-table.bin
```

### ESP32-S3
```
esptool --chip esp32s3 -p xxx --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size detect 0x0 bootloader.bin 0x10000 udpih.bin 0x8000 partition-table.bin
```

You can now disconnect the board from the PC. Make sure to use the dedicated USB port on the board for connecting to the Wii U.
If you power the board externally, you need to reset the board after every UDPIH run.

The board is now flashed and can be used for udpih. Continue with ["Booting the recovery_menu"](../README.md#booting-the-recovery_menu).

## Building from source
Make sure to set up the latest [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).  

Download the latest `arm_kernel.bin.h` from the [releases page](https://github.com/GaryOderNichts/udpih/releases) and copy it to the `arm_kernel` directory (or build it from source).  
You can also simply run:
```bash
curl -L https://github.com/GaryOderNichts/udpih/releases/latest/download/arm_kernel.bin.h > arm_kernel/arm_kernel.bin.h
```

Run `idf.py set-target esp32s2` or `idf.py set-target esp32s3` depending on the target you are building for.

When building the ESP32 code from source, the RGB LED can be enabled for feedback (if the board has one).  
Run `idf.py menuconfig` and navigate to `UDPIH Configuration`. There you can enable the LED and set the GPIO number for your board.

To build the binary run `idf.py build`. Then you can flash UDPIH to your board using `idf.py -p PORT flash`.  
Replace `PORT` with your ESP32 board's USB port name. See the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#build-your-first-project) for more information.
