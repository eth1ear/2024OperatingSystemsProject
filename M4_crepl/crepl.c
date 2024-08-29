#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    static char line[4096];

    while (1) {
        printf("crepl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // 去除输入末尾的换行符
        line[strcspn(line, "\n")] = 0;

        // 检查是否输入了退出命令
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Exiting crepl.\n");
            break; // 退出循环，结束程序
        }

        // 判断输入是否为空
        if (strlen(line) == 0) {
            continue;
        }

        // 判断是否为纯数字输入
        int is_number = 1;
        for (int i = 0; i < strlen(line); i++) {
            if (!isdigit(line[i])) {
                is_number = 0;
                break;
            }
        }

        if (is_number) {
            // 如果输入是数字，直接打印
            printf("%s\n", line);
        } else if (strstr(line, "int ") == line) {
            // 如果是函数定义，将其编译成动态库
            FILE *file = fopen("temp.c", "w");
            fprintf(file, "#include <stdio.h>\n%s\n", line);
            fclose(file);
            if (system("gcc -shared -fPIC -o temp.so temp.c 2>/dev/null") != 0) {
                printf("Compile error.\n");
            } else {
                printf("Function compiled.\n");
            }
        } else {
            // 如果是表达式，尝试编译执行
            FILE *file = fopen("temp_exec.c", "w");
            fprintf(file, "#include <stdio.h>\n#include \"temp.c\"\nint main() { printf(\"%%d\\n\", %s); return 0; }\n", line);
            fclose(file);
            if (system("gcc -o temp_exec temp_exec.c temp.so 2>/dev/null") != 0) {
                printf("Compile error.\n");
            } else {
                system("./temp_exec");
            }
        }
    }

    // 清理生成的临时文件
    remove("temp.c");
    remove("temp.so");
    remove("temp_exec.c");
    remove("temp_exec");

    return 0;
}
