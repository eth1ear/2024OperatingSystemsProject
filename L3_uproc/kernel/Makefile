NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/
CFLAGS 		   += -DDEBUG_LOCAL -DTEST_3

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

export MODULE := L3

include $(AM_HOME)/Makefile
image:

# 通过括号对的检查是否合法
test:
	@make run smp=1 |python3 pc-check.py 10 
