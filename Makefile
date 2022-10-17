
.PHONY: clean all run_qemu disk kernel force_look threaded_compile\
	    native_disk_simu local_disk_install test programs lib remake



DISK_FILE             ?= disk.bin
USED_LOOPBACK         ?= /dev/loop2
QEMU_PATH             ?= qemu-system-x86_64
QEMU_RAM_MB           ?= 256
QEMU_LOG_FILE         ?= qemu.log
QEMU_BIOS_FILE        ?= ./ovmf/OVMF.fd
QEMU_GRAPHICS_OPTIONS ?= -vga virtio
QEMU_TRACE_OPTIONS    ?=


# default: passing -g -fno-inline
# to gcc
# you can change this to 
# 'release' to replace by
# NDEBUG
KERNEL_TARGET ?= debug



QEMU_ARGS := -bios $(QEMU_BIOS_FILE)   \
			 -m    $(QEMU_RAM_MB)      \
			 -D    $(QEMU_LOG_FILE)    \
			 $(QEMU_GRAPHICS_OPTIONS)  \
			 $(QEMU_TRACE_OPTIONS)	   \
			 -d int                    \
			 -M q35                    \
			 -monitor stdio            \
			 -no-reboot  -no-shutdown  \
			 -device nvme,drive=NVME1,serial=deadbeef \
			-drive format=raw,if=none,id=NVME1,file=$(DISK_FILE)
			

#			 -usb \
#			 -device usb-host \




run_qemu: compile image_file local_disk_install
	$(QEMU_PATH) $(QEMU_ARGS)


compile: lib programs kernel


partition_install:
	./write_disk.sh

native_disk_simu: compile
	sudo $(QEMU_PATH) $(QEMU_ARGS) -device nvme,drive=NVME2,serial=dead \
			-drive format=raw,if=none,id=NVME2,file=/dev/nvme0n1


compile: blib programs


test: force_look
	$(MAKE) -C ./tests/


debug: compile
	$(QEMU_PATH) $(QEMU_DEBUG_ARGS)


programs: force_look
	make -C programs

lib: force_look
	make -C blib

threaded_compile:
	make -j compile


$(DISK_FILE): kernel
	dd if=/dev/zero bs=1M count=0 seek=1024 of=$(DISK_FILE)
	sudo /sbin/parted -s $(DISK_FILE) mklabel gpt
	sudo /sbin/parted -s $(DISK_FILE) mkpart Bincows fat32 0% 100%
	sudo /sbin/parted -s $(DISK_FILE) set 1 esp on


image_file: kernel $(DISK_FILE)
	sudo losetup -P $(USED_LOOPBACK) $(DISK_FILE)
	
	sudo /sbin/mkfs.fat -F 32 $(USED_LOOPBACK)p1 -s 8
	mkdir -p img_mount
	sudo mount $(USED_LOOPBACK)p1 img_mount

	sudo cp -r build/* img_mount/
	sync
	sudo umount img_mount
	
	sudo /sbin/losetup -d $(USED_LOOPBACK)
	rm -rf img_mount


	echo ok


remount_disk:
	sudo losetup -P $(USED_LOOPBACK) $(DISK_FILE)
	
	mkdir -p ./disk_output

	sudo mount $(USED_LOOPBACK)p1 ./disk_output/


demount_disk:
	sudo umount ./disk_output/

	sudo /sbin/losetup -d $(USED_LOOPBACK)
	rmdir ./disk_output



kernel: force_look
	$(MAKE) -C ./kernel/ $(KERNEL_TARGET)

clean:
	$(MAKE) clean -C blib
	$(MAKE) clean -C programs
	$(MAKE) clean -C kernel
	rm -f $(DISK_FILE)
	rm -rf build

force_look:
	true


# copy UEFI limine files into build
limine_efi_install:
	LIMINE_VERSION=2.85 ./update_limine.sh


# create build/ tree: main FS layout 
all:
	mkdir -p ./build/
	mkdir -p ./build/bin
	mkdir -p ./build/var/log
	mkdir -p ./build/boot
	mkdir -p ./build/EFI/BOOT
	mkdir -p ./build/home

	cp ./resources/ascii/boot_message.txt ./build/boot
	cp ./resources/cfg/limine.cfg ./build/boot/
	cp ./resources/cfg/default.shrc ./build/home/.shrc

	
	make limine_efi_install
	make programs
	make kernel
