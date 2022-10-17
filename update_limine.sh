#!/bin/sh
# clone limine-bootloader's repository
# in ./limine-bootloader/
# then copy the right files into ./build

mkdir -p ./limine-bootloader/
cd ./limine-bootloader/


if test -d .git; then
    git checkout v$LIMINE_VERSION-binary
    echo checkout
else
    echo clone
    git clone https://github.com/limine-bootloader/limine.git . --branch=v$LIMINE_VERSION-binary
fi
cp BOOTX64.EFI ../build/EFI/BOOT/BOOTX64.EFI
cp limine.sys ../build/boot/limine.sys
