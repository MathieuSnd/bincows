#!/bin/sh

sudo mount /dev/nvme0n1p5 /media/bincows
sudo cp -r disk_root/* /media/bincows/

sudo umount /dev/nvme0n1p5