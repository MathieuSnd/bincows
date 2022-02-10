
.PHONY: clean all run disk kernel force_look threaded_build native_disk

HDD_ROOT := disc_root 
DISK_FILE := disk.bin

PARTITION := /dev/nvme0n1p5

USED_LOOPBACK := /dev/loop2

LIMINE_INSTALL := ./limine-bootloader/limine-install-linux-x86_64

QEMU_PATH := qemu-system-x86_64


QEMU_COMMON_ARGS := -bios /usr/share/ovmf/OVMF.fd \
			 -m 8192 \
			 -M q35 \
			 -vga virtio \
			 -no-reboot  -no-shutdown \
			 -D qemu.log \
			 -trace "pci_nvme_*" \
			 -trace "apic_*" \
			-device nvme,drive=NVME1,serial=deadbeef \
			-drive format=raw,if=none,id=NVME1,file=disk.bin \


			

QEMU_ARGS := -monitor stdio $(QEMU_COMMON_ARGS)
#			 -usb \
#			 -device usb-host \

QEMU_DEBUG_ARGS:= -no-shutdown -s -S -d int $(QEMU_COMMON_ARGS)


run: all
	./write_disk.sh
	$(QEMU_PATH) $(QEMU_ARGS)

native_disk:
	$(QEMU_PATH) $(QEMU_ARGS) 			-device nvme,drive=NVME2,serial=dead \
			-drive format=raw,if=none,id=NVME2,file=/dev/nvme0n1



prun: kernel $(PARTITION)
	sudo $(QEMU_PATH) $(QEMU_ARGS)$(PARTITION)





debug: all
	$(QEMU_PATH) $(QEMU_DEBUG_ARGS)$(DISK_FILE)
	gdb-multiarch -x gdb_cfg
pdebug: $(PARTITION)
	DISK_FILE := $(PARTITION)
	$(QEMU_PATH) $(QEMU_ARGS) 


all: diskfile

threaded_build:
	make -j all


$(PARTITION): kernel
	sudo cp -r disk_root/* /media/bincows/
#sudo $(LIMINE_INSTALL) $(PARTITION)


$(DISK_FILE): kernel/entry.c
	dd if=/dev/zero bs=1M count=0 seek=72 of=$(DISK_FILE)
	sudo /sbin/parted -s $(DISK_FILE) mklabel gpt
	sudo /sbin/parted -s $(DISK_FILE) mkpart Bincows fat32 2048s 100%
	sudo /sbin/parted -s $(DISK_FILE) set 1 esp on
#	$(LIMINE_INSTALL) $(DISK_FILE)

diskfile: kernel $(DISK_FILE)

	sudo losetup -P $(USED_LOOPBACK) $(DISK_FILE)
	
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
	rm -f $(DISK_FILE)

force_look:
	true
