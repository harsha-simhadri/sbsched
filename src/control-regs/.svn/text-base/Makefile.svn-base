# Makefile

obj-m += set_control_regs.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test: all
	sudo insmod ./set_control_regs.ko
	sudo rmmod set_control_regs
	dmesg | tail
