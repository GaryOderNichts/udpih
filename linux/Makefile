ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

obj-m += udpih.o
udpih-objs += main.o utils.o ../common/device.o

ccflags-y += -Wno-unused-variable -Wno-declaration-after-statement

all:
	make -C $(KERNEL_DIR) M=$(ROOT_DIR) modules

clean:
	-make -C $(KERNEL_DIR) M=$(ROOT_DIR) clean
	-rm -f ../common/*.o ../common/.*.o.cmd
