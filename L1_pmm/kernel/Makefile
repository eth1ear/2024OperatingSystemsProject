export MODULE  := L1
NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH := x86_64-qemu
endif

include $(AM_HOME)/Makefile
# include ../Makefile.lab
image: test

.PHONY: test
# 编译测试框架
test:
	@gcc -g $(shell find src/ -name "*.c")  \
		$(shell find test/ -name "*.c") \
		-Iframework -Itest -DTEST -lpthread \
		-o build/test

test0: test
	build/test 0

test1: test
	build/test 1

test2: test
	build/test 2

test3: test
	build/test 3

test4: test
	build/test 4

test5: test
	build/test 5

test6: test
	build/test 6

test7: test
	build/test 7

# 测试所有的测试用例
testall: test
	@build/test 0
	@build/test 1
	@build/test 2
	@build/test 3
	@build/test 4
	@build/test 5
	@build/test 6

all: image, test