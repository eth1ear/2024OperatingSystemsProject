#!/bin/bash
# 获取当前路径
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

make
qemu-system-x86_64 \
	-s -S \
	-serial mon:stdio\
	-machine accel=tcg\
	-nographic\
	-smp ""\
	-drive format=raw,file=$DIR/build/kernel-x86_64-qemu
# serial 将串口connect到我们的标准输入输出流
