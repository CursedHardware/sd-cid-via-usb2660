# Read CID via USB2660

## Introduction

The [SD_USB_CID.c] gets and displays CID register of the SD/MMC card through Microchip's [USB2660] device.

For detailed description of the CID register and its vaues referer JEDEC Standard No.[JESD84-B42]

For specific values check the connected eMMC card datasheet/manual.

## Package Contains

This package contains the following files

1. [SD_USB_CID.c]
2. [sd_usb.h]
3. [sd_usb_win32.h]
4. [README.md]

## Requirements

This file needs the following libraries to be installed

1. [sgutils2](https://github.com/hreinecke/sg3_utils)

sgutils is a scsi driver which will be used to send the SCSI commands to the device.
It can be downloaded from <https://sg.danny.cz/sg/sg3_utils.html>

## Compilation

Use the following command to compile the program

```bash
gcc -g -Wall -o SD_USB_CID SD_USB_CID.c -lpthread -lsgutils2
```

## References

1. [USB82642/2642 SDIO over USB User's Guide.pdf](docs/CN571292.pdf)
2. [USB2660 Linux SDK for CID][usb2660_sdk_cid_register]

The above file can be downloaded from Microchips Website.

[SD_USB_CID.c]: SD_USB_CID.c
[sd_usb.h]: sd_usb.h
[sd_usb_win32.h]: sd_usb_win32.h
[README.md]: README.md
[USB2660]: https://www.microchip.com/product/usb2660
[usb2660_sdk_cid_register]: https://www.microchip.com/en-us/software-library/usb2660_sdk_cid_register
[JESD84-B42]: https://www.jedec.org/sites/default/files/docs/JESD84-B42.pdf
