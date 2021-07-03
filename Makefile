
.PHONY: clean all disk kernel force_look

HDD_ROOT := disc_root 
HDD_FILE := disk.hdd

USED_LOOPBACK := /dev/loop6

all: disk


disk: kernel
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(HDD_FILE)
	sudo /sbin/parted -s $(HDD_FILE) mklabel gpt
	sudo /sbin/parted -s $(HDD_FILE) mkpart ESP fat32 2048s 100%
	sudo /sbin/parted -s $(HDD_FILE) set 1 esp on
	./limine-bootloader/limine-install $(HDD_FILE)

	sudo losetup -P $(USED_LOOPBACK) $(HDD_FILE)
	
	sudo mkfs.fat -F 32 $(USED_LOOPBACK)p1
	mkdir -p img_mount
	sudo mount $(USED_LOOPBACK)p1 img_mount

	sudo cp -r disk_root/* img_mount/
	sync
	sudo umount img_mount
	sudo /sbin/losetup -d $(USED_LOOPBACK)


kernel: force_look
	$(MAKE) -C ./kernel/

clean:
	cd ./kernel/ && make clean
	rm -f $(HDD_FILE)

force_look:
	true
