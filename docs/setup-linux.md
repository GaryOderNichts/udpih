# Linux
## Device specific steps
Some devices require additional setup for UDPIH to work. Click on the device you want to use:

<details>
  <summary><strong>Raspberry Pi Zero (W) / A / A+ / Zero 2 W / 4 / 5 (Expand)</strong></summary>

  > :information_source: For the Pi Zero and Zero 2 W you will need 2 USB cables, one for powering the Zero and one which can be connected to the Wii U.  

  > :information_source: For the Pi 4 and 5 you need to provide power through the power headers. Unfortunately the USB-C port handles both power and USB, and the Pi doesn't boot fast enough when plugged into the Wii U. You cannot use any of the USB-A ports in client mode.

  > :information_source: This guide expects that you use Raspberry Pi OS.

  To use USB gadgets you need to enable `dwc2` by running the command below:  
  > :warning: Prior to Raspberry Pi OS Bookworm, Raspberry Pi OS stored the boot partition at `/boot/`.  

  ```bash
  echo "dtoverlay=dwc2" | sudo tee -a /boot/firmware/config.txt
  ```

  After running the command reboot the system.  

  To install the required dependencies run the command below:  
  ```bash
  sudo apt install git build-essential raspberrypi-kernel-headers
  ```
</details>

<details>
  <summary><strong>Steam Deck (Expand)</strong></summary>

  To build and use UDPIH on the Steam Deck, you need to disable the read-only filesystem and initialize the pacman keyring.
  If you haven't done this before you can follow [this guide](https://steamdecki.org/SteamOS/Read-only_Filesystem).

  Install the required dependencies by running the command below:
  ```bash
  sudo pacman -S base-devel
  ```

  Next you need to install the required linux headers. Start with figuring out the kernel version by running the following command:
  ```bash
  uname -r
  ```
  You'll get an output like this:
  ```bash
  6.1.52-valve16-1-neptune-61
  ```
  In this case you'd want to install the linux headers for neptune-61:
  ```bash
  sudo pacman -S linux-neptune-61-headers # replace neptune-61 with your kernel version
  ```

  Next you'll have to enable USB Dual Role Device in the BIOS:
  - Power off the Steam Deck.
  - Enter the BIOS by holding the Volume Up (+) button and pressing the Power button.
  - Select `Setup Utility`.
  - Navigate to `Advanced` > `USB Configuration` and select `USB Dual Role Device`.
  - Change it from `XHCI` to `DRD`.
  - Navigate to `Exit` and select `Exit Saving Changes`.
</details>

## Building the UDPIH gadget
- Clone the repo:
    ```bash
    git clone https://github.com/GaryOderNichts/udpih.git
    cd udpih
    ```
- Download the latest `arm_kernel.bin.h` from the [releases page](https://github.com/GaryOderNichts/udpih/releases) and copy it to the `arm_kernel` directory.  
  You can also simply run:
    ```bash
    curl -L https://github.com/GaryOderNichts/udpih/releases/latest/download/arm_kernel.bin.h > arm_kernel/arm_kernel.bin.h
    ```
- Now build the kernel module:
    ```bash
    cd linux
    make
    ```

## Running the UDPIH gadget

> :information_source: You need to insert the module again after rebooting the device.  

Run the command below to insert the kernel module into the kernel:
```bash
sudo insmod udpih.ko
```

The device is now ready to be used for udpih.  

Continue with ["Booting the recovery_menu"](../README.md#booting-the-recovery_menu).

## Additional information

If you want to remove the module from the kernel:
```bash
sudo rmmod udpih
```

To show logs and debug information:
```bash
sudo dmesg -w
```
