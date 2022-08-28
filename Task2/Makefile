# ccflags-y := -std=gnu99 -Wno-declaration-after-statement
CONFIG_MODULE_SIG=n

obj-m := final_project_task2.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	if [ -e test.c ]; then gcc -o test test.c; fi

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	if [ -e test ]; then rm -rf test; fi

insmod:
	sudo insmod final_project_task2.ko

rmmod:
	sudo rmmod final_project_task2.ko

