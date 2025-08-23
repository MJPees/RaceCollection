#!/bin/bash
./esptool --chip esp32s3 --port /dev/cu.usbserial-0001 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 ../bin/GhostCar.ino.bootloader.bin 0x8000 ../bin/GhostCar.ino.partitions.bin 0xe000 ../bin/boot_app0.bin 0x10000 ../bin/GhostCar.ino.bin



