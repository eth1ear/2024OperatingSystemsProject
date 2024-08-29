#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_PROCESSES 32768
#define VERSION "pstree (PSmisc) 23.4\nCopyright (C) 1993-2020 Werner Almesberger and Craig Small\nPSmisc 不提供任何保证。\n该程序为自由软件，欢迎你在 GNU 通用公共许可证 (GPL) 下重新发布。\n详情可参阅 COPYING 文件。"

typedef struct Process {
    pid_t pid;
    pid_t ppid;
    char name[256];
} Process;

Process processes[MAX_PROCESSES];
int process_count = 0;
int show_pids = 0;
int numeric_sort = 0;

void read_process_info(pid_t pid) {
    char path[256], buffer[256];
    FILE *status_file;
    Process *process = &processes[process_count++];

    sprintf(path, "/proc/%d/status", pid);
    status_file = fopen(path, "r");
    if (status_file == NULL) {
        process_count--;
        return;
    }

    while (fgets(buffer, sizeof(buffer), status_file)) {
        if (strncmp(buffer, "Pid:", 4) == 0) {
            sscanf(buffer, "Pid: %d", &process->pid);
        } else if (strncmp(buffer, "PPid:", 5) == 0) {
            sscanf(buffer, "PPid: %d", &process->ppid);
        } else if (strncmp(buffer, "Name:", 5) == 0) {
            sscanf(buffer, "Name: %s", process->name);
        }
    }
    fclose(status_file);
}

int compare_processes(const void *a, const void *b) {
    return ((Process*)a)->pid - ((Process*)b)->pid;
}

void print_process_tree(pid_t pid, int level, int last_child) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].ppid == pid) {
            for (int j = 0; j < level; j++) {
                printf(last_child & (1 << j) ? "    " : "│   ");
            }
            printf("├─%s", processes[i].name);
            if (show_pids) {
                printf("(%d)", processes[i].pid);
            }
            printf("\n");

            int has_children = 0;
            for (int k = 0; k < process_count; k++) {
                if (processes[k].ppid == processes[i].pid) {
                    has_children = 1;
                    break;
                }
            }
            print_process_tree(processes[i].pid, level + 1, last_child | (has_children ? 0 : 1 << level));
        }
    }
}

void print_version() {
    printf("%s\n", VERSION);
}

int main(int argc, char *argv[]) {
    DIR *proc_dir;
    struct dirent *entry;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids") == 0) {
            show_pids = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0) {
            numeric_sort = 1;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
    }

    proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("opendir");
        return 1;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            read_process_info(atoi(entry->d_name));
        }
    }
    closedir(proc_dir);

    if (numeric_sort) {
        qsort(processes, process_count, sizeof(Process), compare_processes);
    }

    printf("Process Tree:\n");
    print_process_tree(1, 0, 0);

    return 0;
}

