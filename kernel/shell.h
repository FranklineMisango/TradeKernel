#ifndef SHELL_H
#define SHELL_H

#include "types.h"

#define MAX_COMMAND_LENGTH 256
#define MAX_ARGS 16

// Shell command structure
typedef struct {
    const char* name;
    const char* description;
    void (*handler)(int argc, char* argv[]);
} shell_command_t;

// Shell functions
void shell_init(void);
void shell_run(void);
void shell_process_input(char c);
void shell_execute_command(const char* command_line);

// Built-in commands
void cmd_help(int argc, char* argv[]);
void cmd_clear(int argc, char* argv[]);
void cmd_info(int argc, char* argv[]);
void cmd_mem(int argc, char* argv[]);
void cmd_memstats(int argc, char* argv[]);
void cmd_memleak(int argc, char* argv[]);
void cmd_memcheck(int argc, char* argv[]);
void cmd_pgstats(int argc, char* argv[]);
void cmd_echo(int argc, char* argv[]);
void cmd_reboot(int argc, char* argv[]);
void cmd_ls(int argc, char* argv[]);
void cmd_mkdir(int argc, char* argv[]);
void cmd_touch(int argc, char* argv[]);
void cmd_rm(int argc, char* argv[]);
void cmd_ps(int argc, char* argv[]);
void cmd_schedstat(int argc, char* argv[]);
void cmd_procinfo(int argc, char* argv[]);
void cmd_testfork(int argc, char* argv[]);
void cmd_testipc(int argc, char* argv[]);
void cmd_msgtest(int argc, char* argv[]);

#endif // SHELL_H