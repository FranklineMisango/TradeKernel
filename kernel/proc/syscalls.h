#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "../types.h"

// System call numbers
#define SYS_FORK        0
#define SYS_EXEC        1
#define SYS_EXIT        2
#define SYS_WAIT        3
#define SYS_KILL        4
#define SYS_GETPID      5
#define SYS_GETPPID     6
#define SYS_SLEEP       7
#define SYS_YIELD       8
#define SYS_PIPE        9
#define SYS_READ        10
#define SYS_WRITE       11
#define SYS_CLOSE       12
#define SYS_SHMGET      13
#define SYS_SHMAT       14
#define SYS_SHMDT       15
#define SYS_SHMCTL      16
#define SYS_SETPRIORITY 17
#define SYS_GETPRIORITY 18

#define MAX_SYSCALLS    32

// File descriptor types
typedef enum {
    FD_UNUSED = 0,
    FD_FILE,
    FD_PIPE_READ,
    FD_PIPE_WRITE,
    FD_SOCKET
} fd_type_t;

// Process file descriptor structure  
typedef struct {
    uint8_t in_use;
    fd_type_t type;
    void* data;
    uint32_t flags;
    uint32_t offset;
} proc_file_descriptor_t;

// Pipe structure for IPC
#define PIPE_BUFFER_SIZE 4096

typedef struct {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t size;
    uint32_t capacity;
    uint8_t closed_for_writing;
    uint8_t closed_for_reading;
} pipe_t;

// Shared memory segment
typedef struct {
    uint32_t key;
    uint32_t size;
    void* data;
    uint32_t ref_count;
    uint32_t permissions;
} shm_segment_t;

// Shared memory attachment
typedef struct {
    uint8_t in_use;
    shm_segment_t* segment;
    void* addr;
} shm_attachment_t;

// System call handler function pointer
typedef uint32_t (*syscall_handler_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);

// System call management functions
void syscalls_init(void);
void register_syscall(uint32_t num, syscall_handler_t handler);
uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, 
                        uint32_t arg3, uint32_t arg4);

// System call implementations
uint32_t sys_fork(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_exec(uint32_t program_addr, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_exit(uint32_t exit_code, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_wait(uint32_t child_pid, uint32_t status_ptr, uint32_t arg3, uint32_t arg4);
uint32_t sys_kill(uint32_t pid, uint32_t signal, uint32_t arg3, uint32_t arg4);
uint32_t sys_getpid(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_getppid(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_sleep(uint32_t milliseconds, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_pipe(uint32_t pipefd, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_read(uint32_t fd, uint32_t buffer, uint32_t count, uint32_t arg4);
uint32_t sys_write(uint32_t fd, uint32_t buffer, uint32_t count, uint32_t arg4);
uint32_t sys_close(uint32_t fd, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_shmget(uint32_t key, uint32_t size, uint32_t flags, uint32_t arg4);
uint32_t sys_shmat(uint32_t shmid, uint32_t addr, uint32_t flags, uint32_t arg4);
uint32_t sys_shmdt(uint32_t addr, uint32_t arg2, uint32_t arg3, uint32_t arg4);
uint32_t sys_shmctl(uint32_t shmid, uint32_t cmd, uint32_t buf, uint32_t arg4);
uint32_t sys_setpriority(uint32_t pid, uint32_t priority, uint32_t arg3, uint32_t arg4);
uint32_t sys_getpriority(uint32_t pid, uint32_t arg2, uint32_t arg3, uint32_t arg4);

// Assembly system call interface
extern void syscall_interrupt_handler(void);

// Helper functions for user space
static inline uint32_t syscall(uint32_t num, uint32_t arg1, uint32_t arg2, 
                              uint32_t arg3, uint32_t arg4) {
    uint32_t result;
    asm volatile("int $0x80"
                : "=a" (result)
                : "a" (num), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4)
                : "memory");
    return result;
}

// User-friendly wrappers
static inline int fork(void) {
    return syscall(SYS_FORK, 0, 0, 0, 0);
}

static inline int exec(void* program) {
    return syscall(SYS_EXEC, (uint32_t)program, 0, 0, 0);
}

static inline void exit(int code) {
    syscall(SYS_EXIT, code, 0, 0, 0);
}

static inline int wait(int* status) {
    return syscall(SYS_WAIT, 0, (uint32_t)status, 0, 0);
}

static inline int kill(int pid, int signal) {
    return syscall(SYS_KILL, pid, signal, 0, 0);
}

static inline int getpid(void) {
    return syscall(SYS_GETPID, 0, 0, 0, 0);
}

static inline int getppid(void) {
    return syscall(SYS_GETPPID, 0, 0, 0, 0);
}

static inline int sleep(int milliseconds) {
    return syscall(SYS_SLEEP, milliseconds, 0, 0, 0);
}

static inline void yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0);
}

static inline int pipe(int pipefd[2]) {
    return syscall(SYS_PIPE, (uint32_t)pipefd, 0, 0, 0);
}

static inline int read(int fd, void* buffer, int count) {
    return syscall(SYS_READ, fd, (uint32_t)buffer, count, 0);
}

static inline int write(int fd, const void* buffer, int count) {
    return syscall(SYS_WRITE, fd, (uint32_t)buffer, count, 0);
}

static inline int close(int fd) {
    return syscall(SYS_CLOSE, fd, 0, 0, 0);
}

static inline int setpriority(int pid, int priority) {
    return syscall(SYS_SETPRIORITY, pid, priority, 0, 0);
}

static inline int getpriority(int pid) {
    return syscall(SYS_GETPRIORITY, pid, 0, 0, 0);
}

#endif