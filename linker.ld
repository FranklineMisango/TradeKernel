/* TradeKernel Linker Script
 * Optimized memory layout for ultra-low latency execution
 */

ENTRY(kernel_main)

SECTIONS
{
    /* Kernel starts at 1MB physical address */
    . = 0x100000;
    
    /* Code section - hot path code first */
    .text ALIGN(4K) : {
        _text_start = .;
        
        /* Critical hot path functions first for better cache locality */
        *(.text.hot)
        *(.text.likely)
        
        /* Entry point */
        *(.text.startup)
        
        /* Main kernel code */
        *(.text)
        *(.text.*)
        
        /* Read-only data immediately after code for cache efficiency */
        *(.rodata)
        *(.rodata.*)
        
        _text_end = .;
    }
    
    /* Initialized data section */
    .data ALIGN(4K) : {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }
    
    /* Uninitialized data section */
    .bss ALIGN(4K) : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }
    
    /* Stack section - separate page for protection */
    .stack ALIGN(4K) : {
        _stack_bottom = .;
        . += 0x4000; /* 16KB stack */
        _stack_top = .;
    }
    
    /* Heap section for dynamic allocation */
    .heap ALIGN(4K) : {
        _heap_start = .;
        . += 0x100000; /* 1MB heap */
        _heap_end = .;
    }
    
    /* Performance-critical data structures */
    .critical_data ALIGN(64) : { /* Cache line aligned */
        _critical_data_start = .;
        *(.data.critical)
        *(.bss.critical)
        _critical_data_end = .;
    }
    
    /* Network buffers - DMA aligned */
    .net_buffers ALIGN(4K) : {
        _net_buffers_start = .;
        . += 0x200000; /* 2MB for network buffers */
        _net_buffers_end = .;
    }
    
    /* Task stacks - each aligned to page boundary */
    .task_stacks ALIGN(4K) : {
        _task_stacks_start = .;
        . += 0x400000; /* 4MB for task stacks */
        _task_stacks_end = .;
    }
    
    /* Memory pool section */
    .memory_pools ALIGN(4K) : {
        _memory_pools_start = .;
        . += 0x800000; /* 8MB for memory pools */
        _memory_pools_end = .;
    }
    
    /* Debug information (only in debug builds) */
    .debug_info : {
        *(.debug_info)
        *(.debug_abbrev)
        *(.debug_line)
        *(.debug_str)
        *(.debug_ranges)
    }
    
    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.note*)
        *(.comment)
        *(.eh_frame*)
        *(.gcc_except_table)
    }
    
    /* Kernel end marker */
    _kernel_end = .;
    
    /* Total kernel size */
    _kernel_size = _kernel_end - 0x100000;
}
