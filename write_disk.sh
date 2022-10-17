#!/bin/sh

if [ -z "$BINCOWS_PART" ]; then
    echo "\$BINCOWS_PART should be defined";
    exit 2;
fi

sudo dd if=/dev/zero bs=1M count=0 seek=200 of=$BINCOWS_PART


make -p /media/bincows_part

sudo mkfs.fat -F 32 $BINCOWS_PART -s 8
sudo mount $BINCOWS_PART /media/bincows_part
sudo rm -rf /media/bincows_part/*
sudo rm -rf /media/bincows_part/.*
sudo cp -r build/* /media/bincows_part/
sync
sudo umount $BINCOWS_PART
