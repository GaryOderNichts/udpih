# USB Descriptor Parsing Is Hard (UDPIH)
Exploits the Wii U's USB Host Stack descriptor parsing. Pronounced like "mud pie" without the M.

[The write-up can be found here!](https://garyodernichts.blogspot.com/2022/06/exploiting-wii-us-usb-descriptor-parsing.html)

## Requirements
- A Wii U
- One of the devices listed below

    *Note: Any other linux device capable of USB device emulation should work as well.  
    Prebuilt releases and instructions are only available for the Pico and Zero.  
    I will add more devices below which are confirmed to work.*

### Supported devices:
- A Raspberry Pi Pico or Zero
- A Nintendo Switch capable of running [udpih_nxpayload](https://github.com/GaryOderNichts/udpih_nxpayload)

## Instructions
### Pico
- Download the latest `udpih.uf2` from the [releases page](https://github.com/GaryOderNichts/udpih/releases).
- Hold down the `BOOTSEL` button on the board and connect the Pico to your PC.  
  Your PC will detect the Pi as a storage device.
- Copy the `.uf2` file to the Pico. It will disconnect after a few seconds.

The Pico is now flashed and can be used for udpih. Continue with ["Booting the recovery_menu"](#booting-the-recoverymenu) below.

### Raspberry Pi Zero (Linux)
> :information_source: To use USB gadgets on the Pi Zero you need to enable the `dwc2` module by running the commands below:  
> `echo "dtoverlay=dwc2" | sudo tee -a /boot/config.txt`  
> `echo "dwc2" | sudo tee -a /etc/modules`  
> After running the commands reboot the system.

- Install the required dependencies:
    ```bash
    sudo apt install build-essential raspberrypi-kernel-headers
    ```
- Clone the repo:
    ```bash
    git clone https://github.com/GaryOderNichts/udpih.git
    cd udpih
    ```
- Download the latest `arm_kernel.bin.h` from the [releases page](https://github.com/GaryOderNichts/udpih/releases) and copy it to the `arm_kernel` directory.
- Now build the kernel module:
    ```bash
    cd linux
    make
    ```
- You can now run `sudo insmod udpih.ko` to insert the kernel module into the kernel.

The Zero is now ready to be used for udpih.  
Note that you'll need to insert the module again after rebooting the Zero. You will need 2 USB cables, one for powering the Zero and one which can be connected to the Wii U.

Continue with ["Booting the recovery_menu"](#booting-the-recoverymenu) below.

### Booting the recovery_menu
> :warning: Important notes for this to work:
> - Make sure **no** other USB Devices are attached to the console.
> - Only use USB ports on the front of the console, the back ports **will not** work.
> - If your console has standby mode enabled, pull the power plug and turn it on from a full coldboot state.

- Copy the latest release of the [recovery_menu](https://github.com/GaryOderNichts/recovery_menu/releases) to the root of your FAT32 formatted SD Card.
- Insert the SD Card into the console and power it on.
- As soon as you see the "Wii U" logo on the TV or Gamepad plug in your Zero/Pico.  
  This timing is important. If you're already in the menu, the exploit won't work..
- After a few seconds you should be in the recovery menu.

Check out the [recovery_menu README](https://github.com/GaryOderNichts/recovery_menu) for more information about this menu.

## Building
```bash
# build the docker container
docker build -t udpihbuilder .

# build the pico code
docker run -it --rm -v ${PWD}:/project udpihbuilder make pico

# to only build the arm kernel code
docker run -it --rm -v ${PWD}:/project udpihbuilder make arm_kernel
```

**Special thanks to Maschell, rw-r-r-0644, QuarkTheAwesome, vgmoose, exjam, dimok789, and everyone else who contributed to the Wii U scene!**
