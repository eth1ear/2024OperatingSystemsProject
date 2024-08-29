# 编译器和编译选项
CC = gcc
CFLAGS = -pthread -O2
TARGET = plcs

# 测试用例文件
TEST_CASES = tests.txt

# 默认目标
all: $(TARGET) test

# 编译目标程序
$(TARGET): plcs.c
	$(CC) $(CFLAGS) -o $(TARGET) plcs.c

# 运行自动化测试
test: $(TARGET)
	@python3 test.py

# 清理编译文件和生成的目标文件
clean:
	rm -f $(TARGET)
	rm -f *.o

# 伪目标 (不生成文件)
.PHONY: all test clean
