# USB Descriptor Parsing Is Hard (UDPIH)
Exploits the Wii U's USB Host Stack descriptor parsing. Pronounced like "mud pie" without the M.

[The write-up can be found here!](https://garyodernichts.blogspot.com/2022/06/exploiting-wii-us-usb-descriptor-parsing.html)

## Requirements
- A Wii U
- One of the devices listed below

    *Note: Any other linux device capable of USB device emulation should work as well.  
    Prebuilt releases and instructions are only available for tested devices.
    I will add more devices below which are confirmed to work.*

### Supported devices:
- Raspberry Pi Pico (W) / Pico 2 (W)
- Raspberry Pi Zero (W) / A / A+ / Zero 2 W / 4 / 5
- Steam Deck
- Espressif ESP32 S2 / S3
- Nintendo Switch capable of running [udpih_nxpayload](https://github.com/GaryOderNichts/udpih_nxpayload)

## Instructions
### Device Setup
Follow the setup guide for the device you want to use below:
- [Raspberry Pi Pico / Pico 2](./docs/setup-pico.md)
- [Raspberry Pi Zero (W) / A / A+ / Zero 2 W / 4 / 5](./docs/setup-linux.md)
- [Steam Deck](./docs/setup-linux.md)
- [Espressif ESP32 S2 / S3](./docs/setup-esp32.md)
- [Nintendo Switch](https://github.com/GaryOderNichts/udpih_nxpayload#Instructions)

### Booting the recovery_menu
> [!IMPORTANT]  
> Important notes for this to work:
> - Make sure **no** other USB devices are attached to the console.
> - Only use USB ports on the **front** of the console, the back ports **will not** work.
> - If your console has standby mode enabled, pull the power plug and turn it on from a full coldboot state.

- Copy the latest release of the [recovery_menu](https://github.com/GaryOderNichts/recovery_menu/releases) to the root of your FAT32 formatted SD Card.
- Insert the SD Card into the console and power it on.
- As soon as you see the "Wii U" logo on the TV or Gamepad plug in your prepared UDPIH device.  
  This timing is important. If you're already in the menu, the exploit won't work.  
  Depending on the device, you might have to plug it in sooner or later. This might take several attempts.  
  If you get no video output or a distorted screen, your timing was most likely wrong.
- After a few seconds you should be in the recovery menu.

> [!NOTE]
> If your console turns off after performing the exploit, the `recovery_menu` file couldn't be loaded from your Wii U's SD Card.

Check out the [recovery_menu README](https://github.com/GaryOderNichts/recovery_menu) for more information about this menu.

## Building
Building the kernel code requires devkitARM. Device specific instructions can be found in the documentation.

```bash
# only build the arm kernel code
make arm_kernel
```

**Special thanks to Maschell, rw-r-r-0644, QuarkTheAwesome, vgmoose, exjam, dimok789, and everyone else who contributed to the Wii U scene!**
