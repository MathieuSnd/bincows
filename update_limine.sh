#!/bin/sh
cd ./limine-bootloader/
rm -rf *
rm -rf .*

git clone https://github.com/limine-bootloader/limine.git . --branch=$V-binary
cp BOOTX64.EFI ../disk_root/EFI/BOOT/BOOTX64.EFI
cp limine.sys ../disk_root/boot/limine.sys

cd ..

make clean && make