#include "syscalls.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../mm/memory.h"
#include "../drivers/vga.h"
#include "../arch/interrupts.h"

// Remove static memcpy/memset implementations - use the ones from memory.h

// System call table
static syscall_handler_t syscall_table[MAX_SYSCALLS];
static uint32_t num_syscalls = 0;

// Current process pointer (will be set by scheduler)
extern process_t* current_process;

void syscalls_init(void) {
    memset(syscall_table, 0, sizeof(syscall_table));
    
    // Register core system calls (only the ones that work)
    register_syscall(SYS_FORK, sys_fork);
    register_syscall(SYS_EXIT, sys_exit);
    register_syscall(SYS_WAIT, sys_wait);
    register_syscall(SYS_KILL, sys_kill);
    register_syscall(SYS_GETPID, sys_getpid);
    register_syscall(SYS_YIELD, sys_yield);
    
    // TODO: Enable these when process structure is updated
    // register_syscall(SYS_GETPPID, sys_getppid);
    // register_syscall(SYS_SLEEP, sys_sleep);
    // register_syscall(SYS_EXEC, sys_exec);
    // register_syscall(SYS_PIPE, sys_pipe);
    // register_syscall(SYS_READ, sys_read);
    // register_syscall(SYS_WRITE, sys_write);
    // register_syscall(SYS_CLOSE, sys_close);
    // register_syscall(SYS_SHMGET, sys_shmget);
    // register_syscall(SYS_SHMAT, sys_shmat);
    // register_syscall(SYS_SHMDT, sys_shmdt);
    // register_syscall(SYS_SHMCTL, sys_shmctl);
    // register_syscall(SYS_SETPRIORITY, sys_setpriority);
    // register_syscall(SYS_GETPRIORITY, sys_getpriority);
    
    vga_write_string("System calls initialized\n");
}

void register_syscall(uint32_t num, syscall_handler_t handler) {
    if (num < MAX_SYSCALLS) {
        syscall_table[num] = handler;
        if (num >= num_syscalls) {
            num_syscalls = num + 1;
        }
    }
}

uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, 
                        uint32_t arg3, uint32_t arg4) {
    if (syscall_num >= num_syscalls || !syscall_table[syscall_num]) {
        return -1; // Invalid system call
    }
    
    return syscall_table[syscall_num](arg1, arg2, arg3, arg4);
}

// System call implementations
uint32_t sys_fork(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; // Suppress unused parameter warnings
    
    if (!current_process) {
        return -1;
    }
    
    // Create child process with same priority as parent
    char child_name[32] = "child";
    process_t* child = process_create(child_name, NULL, current_process->priority);
    if (!child) {
        return -1;
    }
    
    // Copy parent's context (simplified fork)
    memcpy(&child->context, &current_process->context, sizeof(cpu_context_t));
    
    // Set return values: parent gets child PID, child gets 0
    child->context.eax = 0;
    current_process->context.eax = child->pid;
    
    // Set up parent-child relationship
    child->ppid = current_process->pid;
    child->parent = current_process;
    
    return child->pid;
}

// TODO: Implement when exec functionality is added
// uint32_t sys_exec(uint32_t program_addr, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     if (!current_process) {
//         return -1;
//     }
    
//     // For now, just a placeholder - would load and execute new program
//     // In a full implementation, this would:
//     // 1. Load program from storage
//     // 2. Set up new memory space
//     // 3. Replace current process image
//     // 4. Jump to entry point
    
//     return -1;
// }

uint32_t sys_exit(uint32_t exit_code, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    (void)arg2; (void)arg3; (void)arg4; // Suppress unused parameter warnings
    
    if (!current_process) {
        return -1;
    }
    
    process_exit(current_process, exit_code);
    
    // This should not return as process is terminated
    scheduler_yield();
    return 0;
}

uint32_t sys_wait(uint32_t child_pid, uint32_t status_ptr, uint32_t arg3, uint32_t arg4) {
    (void)arg3; (void)arg4; // Suppress unused parameter warnings
    
    if (!current_process) {
        return -1;
    }
    
    // Find child process
    process_t* child = process_find_by_pid(child_pid);
    if (!child || child->ppid != current_process->pid) {
        return -1;
    }
    
    // If child is still running, wait for it
    if (child->state != PROCESS_TERMINATED && child->state != PROCESS_ZOMBIE) {
        // Block current process until child terminates
        process_block(current_process);
        return 0; // Will be rescheduled when child terminates
    }
    
    // Child has terminated, collect exit status
    if (status_ptr) {
        *(int32_t*)status_ptr = child->exit_code;
    }
    
    uint32_t child_pid_ret = child->pid;
    process_destroy(child);
    
    return child_pid_ret;
}

uint32_t sys_kill(uint32_t pid, uint32_t signal, uint32_t arg3, uint32_t arg4) {
    (void)arg3; (void)arg4; // Suppress unused parameter warnings
    
    return process_kill(pid, signal);
}

uint32_t sys_getpid(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    return current_process ? current_process->pid : 0;
}

// TODO: Implement when parent_pid field is added to process structure
// uint32_t sys_getppid(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     return current_process ? current_process->parent_pid : 0;
// }

// TODO: Implement when sleep_until field is added to process structure
// uint32_t sys_sleep(uint32_t milliseconds, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     if (!current_process) {
//         return -1;
//     }
    
//     current_process->sleep_until = get_ticks() + (milliseconds / 10); // Assume 10ms tick
//     current_process->state = PROCESS_SLEEPING;
//     scheduler_yield();
    
//     return 0;
// }

uint32_t sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    scheduler_yield();
    return 0;
}

// TODO: Implement when file descriptors are added to process structure
// uint32_t sys_pipe(uint32_t pipefd, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     if (!current_process || !pipefd) {
//         return -1;
//     }
    
//     // Create pipe - simplified implementation
//     pipe_t* pipe = kmalloc(sizeof(pipe_t));
//     if (!pipe) {
//         return -1;
//     }
    
//     pipe->read_pos = 0;
//     pipe->write_pos = 0;
//     pipe->size = 0;
//     pipe->capacity = PIPE_BUFFER_SIZE;
    
//     // Find two free file descriptors
//     int read_fd = -1, write_fd = -1;
//     for (int i = 0; i < MAX_OPEN_FILES; i++) {
//         if (!current_process->open_files[i].in_use) {
//             if (read_fd == -1) {
//                 read_fd = i;
//             } else if (write_fd == -1) {
//                 write_fd = i;
//                 break;
//             }
//         }
//     }
    
//     if (read_fd == -1 || write_fd == -1) {
//         kfree(pipe);
//         return -1;
//     }
    
//     // Set up file descriptors
//     current_process->open_files[read_fd].in_use = 1;
//     current_process->open_files[read_fd].type = FD_PIPE_READ;
//     current_process->open_files[read_fd].data = pipe;
    
//     current_process->open_files[write_fd].in_use = 1;
//     current_process->open_files[write_fd].type = FD_PIPE_WRITE;
//     current_process->open_files[write_fd].data = pipe;
    
//     // Return file descriptors to user
//     ((int*)pipefd)[0] = read_fd;
//     ((int*)pipefd)[1] = write_fd;
    
//     return 0;
// }

// TODO: Implement when file descriptors are added to process structure
// uint32_t sys_read(uint32_t fd, uint32_t buffer, uint32_t count, uint32_t arg4) {
//     if (!current_process || fd >= MAX_OPEN_FILES || !current_process->open_files[fd].in_use) {
//         return -1;
//     }
    
//     file_descriptor_t* file_desc = &current_process->open_files[fd];
    
//     if (file_desc->type == FD_PIPE_READ) {
//         pipe_t* pipe = (pipe_t*)file_desc->data;
//         uint32_t bytes_read = 0;
//         uint8_t* buf = (uint8_t*)buffer;
        
//         while (bytes_read < count && pipe->size > 0) {
//             buf[bytes_read] = pipe->buffer[pipe->read_pos];
//             pipe->read_pos = (pipe->read_pos + 1) % pipe->capacity;
//             pipe->size--;
//             bytes_read++;
//         }
        
//         return bytes_read;
//     }
    
//     return -1;
// }

// TODO: Implement when file descriptors are added to process structure
// uint32_t sys_write(uint32_t fd, uint32_t buffer, uint32_t count, uint32_t arg4) {
//     if (!current_process || fd >= MAX_OPEN_FILES || !current_process->open_files[fd].in_use) {
//         return -1;
//     }
    
//     file_descriptor_t* file_desc = &current_process->open_files[fd];
    
//     if (file_desc->type == FD_PIPE_WRITE) {
//         pipe_t* pipe = (pipe_t*)file_desc->data;
//         uint32_t bytes_written = 0;
//         uint8_t* buf = (uint8_t*)buffer;
        
//         while (bytes_written < count && pipe->size < pipe->capacity) {
//             pipe->buffer[pipe->write_pos] = buf[bytes_written];
//             pipe->write_pos = (pipe->write_pos + 1) % pipe->capacity;
//             pipe->size++;
//             bytes_written++;
//         }
        
//         return bytes_written;
//     } else if (fd == 1) { // stdout
//         for (uint32_t i = 0; i < count; i++) {
//             vga_print_char(((char*)buffer)[i]);
//         }
//         return count;
//     }
    
//     return -1;
// }

// TODO: Implement when file descriptors are added to process structure
// uint32_t sys_close(uint32_t fd, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     if (!current_process || fd >= MAX_OPEN_FILES || !current_process->open_files[fd].in_use) {
//         return -1;
//     }
    
//     file_descriptor_t* file_desc = &current_process->open_files[fd];
    
//     if (file_desc->type == FD_PIPE_READ || file_desc->type == FD_PIPE_WRITE) {
//         // For pipes, would need reference counting in full implementation
//         if (file_desc->data) {
//             kfree(file_desc->data);
//         }
//     }
    
//     file_desc->in_use = 0;
//     file_desc->type = FD_UNUSED;
//     file_desc->data = NULL;
    
//     return 0;
// }

// TODO: Implement when shared memory is added to process structure
// uint32_t sys_shmget(uint32_t key, uint32_t size, uint32_t flags, uint32_t arg4) {
//     // Simplified shared memory implementation
//     shm_segment_t* segment = kmalloc(sizeof(shm_segment_t));
//     if (!segment) {
//         return -1;
//     }
    
//     segment->key = key;
//     segment->size = size;
//     segment->data = kmalloc(size);
//     segment->ref_count = 0;
    
//     if (!segment->data) {
//         kfree(segment);
//         return -1;
//     }
    
//     // Return segment ID (simplified)
//     return (uint32_t)segment;
// }

// uint32_t sys_shmat(uint32_t shmid, uint32_t addr, uint32_t flags, uint32_t arg4) {
//     shm_segment_t* segment = (shm_segment_t*)shmid;
//     if (!segment || !current_process) {
//         return -1;
//     }
    
//     // Find free shared memory slot
//     for (int i = 0; i < MAX_SHARED_MEMORY; i++) {
//         if (!current_process->shared_memory[i].in_use) {
//             current_process->shared_memory[i].in_use = 1;
//             current_process->shared_memory[i].segment = segment;
//             current_process->shared_memory[i].addr = segment->data;
//             segment->ref_count++;
//             return (uint32_t)segment->data;
//         }
//     }
    
//     return -1;
// }

// uint32_t sys_shmdt(uint32_t addr, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     if (!current_process) {
//         return -1;
//     }
    
//     // Find and detach shared memory
//     for (int i = 0; i < MAX_SHARED_MEMORY; i++) {
//         if (current_process->shared_memory[i].in_use && 
//             current_process->shared_memory[i].addr == (void*)addr) {
            
//             current_process->shared_memory[i].segment->ref_count--;
//             current_process->shared_memory[i].in_use = 0;
//             current_process->shared_memory[i].segment = NULL;
//             current_process->shared_memory[i].addr = NULL;
//             return 0;
//         }
//     }
    
//     return -1;
// }

// uint32_t sys_shmctl(uint32_t shmid, uint32_t cmd, uint32_t buf, uint32_t arg4) {
//     shm_segment_t* segment = (shm_segment_t*)shmid;
//     if (!segment) {
//         return -1;
//     }
    
//     if (cmd == 0) { // IPC_RMID - remove segment
//         if (segment->ref_count == 0) {
//             kfree(segment->data);
//             kfree(segment);
//             return 0;
//         }
//     }
    
//     return -1;
// }

// TODO: Implement when priority scheduling is added
// uint32_t sys_setpriority(uint32_t pid, uint32_t priority, uint32_t arg3, uint32_t arg4) {
//     process_t* process = pid ? process_find_by_pid(pid) : current_process;
//     if (!process) {
//         return -1;
//     }
    
//     if (priority >= PRIORITY_REALTIME && priority <= PRIORITY_IDLE) {
//         process->priority = priority;
//         return 0;
//     }
    
//     return -1;
// }

// uint32_t sys_getpriority(uint32_t pid, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
//     process_t* process = pid ? process_find_by_pid(pid) : current_process;
//     if (!process) {
//         return -1;
//     }
    
//     return process->priority;
// }