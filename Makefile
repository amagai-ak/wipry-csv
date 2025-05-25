OBJECTS = main.o

EXEC = wipry-csv

BUILDTIMESTAMP = \"`date -u +"%Y-%m-%dT%H:%M:%SZ"`\"
CC = gcc
CXX = g++
FLAGS = -Wall -g -I./ -L./ -std=c++11 -dD -D__BUILDTIMESTAMP__=$(BUILDTIMESTAMP)
LIBS = -lWiPryClarity -lusb-1.0 -lpthread

QEMUDIR=./arm64-root
SRCFILES = main.cpp \
		   WiPryClarity.h \
		   libWiPryClarity.a \
		   Makefile

$(EXEC): $(OBJECTS)
	$(CXX) $(FLAGS) -o $(EXEC) $(OBJECTS) $(LIBS)

.c.o:
	$(CC) -c $(FLAGS) $<

.cpp.o:
	$(CXX) -c $(FLAGS) $<

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(EXEC)

.PHONY: prepare
prepare:
	sudo apt -y install libusb-1.0-0-dev libpcap-dev libpcap0.8-dev libpcap0.8 libusb-0.1-4 libusb-1.0-0-dev libusb-1.0-0 libusb-dev

.PHONY: qemu_env
qemu_env:
	sudo apt -y install qemu-user-static debootstrap binfmt-support
	sudo debootstrap --arch=arm64 --foreign jammy $(QEMUDIR) http://ports.ubuntu.com/
	sudo cp /usr/bin/qemu-aarch64-static $(QEMUDIR)/usr/bin/
	sudo chroot $(QEMUDIR) /debootstrap/debootstrap --second-stage
	sudo chroot $(QEMUDIR) apt update
	sudo chroot $(QEMUDIR) apt -y install build-essential usbutils libstdc++6 libssl3 libcurl4
	sudo chroot $(QEMUDIR) apt -y install libusb-1.0-0-dev libpcap-dev libpcap0.8-dev libpcap0.8 libusb-0.1-4 libusb-1.0-0-dev libusb-1.0-0 libusb-dev

.PHONY: qemu_build
qemu_build:
	sudo mkdir -p $(QEMUDIR)/root/wipry-csv
	sudo cp $(SRCFILES) $(QEMUDIR)/root/wipry-csv/.
	sudo chroot $(QEMUDIR) make -C /root/wipry-csv

.PHONY: qemu_mount
qemu_mount:
	sudo mount --bind /dev $(QEMUDIR)/dev
	sudo mount --bind /sys $(QEMUDIR)/sys

.PHONY: qemu_umount
qemu_umount:
	sudo umount $(QEMUDIR)/dev
	sudo umount $(QEMUDIR)/sys

.PHONY: qemu_run
qemu_run: qemu_mount
	sudo chroot $(QEMUDIR) /root/wipry-csv/$(EXEC) -c -2
