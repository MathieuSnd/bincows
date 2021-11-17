
.PHONY: clean all run disk kernel force_look threaded_build

HDD_ROOT := disc_root 
HDD_FILE := disk.hdd
 
USED_LOOPBACK := /dev/loop6

LIMINE_INSTALL := ./limine-bootloader/limine-install-linux-x86_64

QEMU_PATH := qemu-system-x86_64
QEMU_ARGS := -monitor stdio -bios /usr/share/ovmf/OVMF.fd -m 114 -vga std -no-reboot

run: all
	$(QEMU_PATH) $(QEMU_ARGS) -drive format=raw,file=$(HDD_FILE),id=disk,if=none \
									-device ahci,id=ahci \
									-device ide-hd,drive=disk,bus=ahci.0

all: disk

threaded_build:
	make -j all

$(HDD_FILE): kernel/entry.c
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(HDD_FILE)
	sudo /sbin/parted -s $(HDD_FILE) mklabel gpt
	sudo /sbin/parted -s $(HDD_FILE) mkpart ESP fat32 2048s 100%
	sudo /sbin/parted -s $(HDD_FILE) set 1 esp on
	$(LIMINE_INSTALL) $(HDD_FILE)

disk: kernel $(HDD_FILE)

	sudo losetup -P $(USED_LOOPBACK) $(HDD_FILE)
	
	sudo mkfs.fat -F 32 $(USED_LOOPBACK)p1
	mkdir -p img_mount
	sudo mount $(USED_LOOPBACK)p1 img_mount

	sudo cp -r disk_root/* img_mount/
	sync
	sudo umount img_mount
	
	sudo /sbin/losetup -d $(USED_LOOPBACK)
	rm -rf img_mount


kernel: force_look
	$(MAKE) -C ./kernel/

clean:
	cd ./kernel/ && make clean
	rm -f $(HDD_FILE)

force_look:
	true
