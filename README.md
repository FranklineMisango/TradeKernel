# TradeKernel
**Bare-Metal Real-Time OS for Ultra-Low-Latency Trading**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/yourusername/tradekernel)
[![Demo Ready](https://img.shields.io/badge/demo-ready-blue.svg)](./demo.sh)
[![Trading System](https://img.shields.io/badge/trading-functional-orange.svg)](./TRADING_DEMO.md)

## Overview
TradeKernel is a deterministic operating system engineered for high-frequency trading (HFT), written in C++ and x86_64 Assembly. It eliminates traditional OS jitter through a custom tickless scheduler, kernel-bypass networking, and pre-allocated memory pools to achieve sub-microsecond latency.

## Features

- **32-bit Protected Mode**: Complete transition from 16-bit real mode
- **VGA Text Mode Driver**: 80x25 color text display with scrolling
- **Memory Management**: Simple heap allocator with malloc/free functionality
- **Interrupt Handling**: Keyboard and timer interrupt support
- **Interactive Console**: Real-time keyboard input processing
- **Clean Architecture**: Modular design with separate drivers and subsystems

## Architecture

```
kernel/
├── arch/                    # Architecture-specific code
│   ├── boot.asm            # Bootloader (16-bit -> 32-bit transition)
│   ├── interrupts.h/.c     # Interrupt descriptor table and handlers
│   └── interrupt_handlers.asm # Assembly interrupt wrappers
├── drivers/                 # Device drivers
│   ├── vga.h/.c            # VGA text mode driver
├── mm/                     # Memory management
│   └── memory.h/.c         # Heap allocator and memory utilities
├── kernel.c                # Main kernel entry point
└── kernel.ld               # Linker script
```

## Building and Running

### Prerequisites

Install the required development tools:

```bash
sudo apt-get update
sudo apt-get install build-essential nasm qemu-system-x86
```

### Quick Start

1. **Clone and navigate to the project:**
   ```bash
   cd TradeKernel-ArithmaX-Customized
   ```

2. **Build and run the OS:**
   ```bash
   ./run_tradeos.sh
   ```

3. **For quick development testing:**
   ```bash
   ./quick_test.sh
   ```

### Manual Build

```bash
# Clean previous builds
make -f Makefile.new clean

# Build the OS image
make -f Makefile.new all

# Run in QEMU
make -f Makefile.new run
```

### QEMU Controls

- **Ctrl+Alt+G**: Release mouse from QEMU window
- **Ctrl+Alt+2**: Switch to QEMU monitor console
- **Ctrl+Alt+1**: Return to OS display
- **Ctrl+C**: Quit QEMU (in terminal)

## System Specifications

- **Target Architecture**: x86 (32-bit)
- **Memory Layout**:
  - Bootloader: 0x7C00 (loaded by BIOS)
  - Kernel: 0x10000 (64KB)
  - Kernel Heap: 0x100000 (1MB+, 4MB allocated)
- **VGA Text Mode**: 80x25 characters, 16 colors
- **Interrupts**: Timer (IRQ 0) and Keyboard (IRQ 1)

## Development

### Adding New Features

1. **Drivers**: Add new device drivers in `kernel/drivers/`
2. **System Calls**: Extend interrupt handling in `kernel/arch/`
3. **Memory**: Enhance memory management in `kernel/mm/`
4. **Algorithms**: Build trading algorithms on top of the kernel foundation

### Debugging

Use QEMU's debugging features:

```bash
# Start with GDB server
make -f Makefile.new debug

# In another terminal, connect with GDB
gdb
(gdb) target remote :1234
(gdb) symbol-file build/kernel.bin
```

## File Structure

- `run_tradeos.sh`: Main build and run script
- `quick_test.sh`: Quick development testing
- `Makefile.new`: Build system configuration
- `kernel/`: All kernel source code
- `build/`: Generated build artifacts (created during build)

## Testing

The OS provides an interactive console where you can:

- Type characters using the keyboard
- See real-time text output
- Observe interrupt handling in action
- Test memory allocation (can be extended)

## Development Roadmap

### Phase 1: Core System Features (Essential)

#### 1. File System ✅ **COMPLETED**
- **Simple FAT-like filesystem** for storing trading strategies and data ✅
- **Basic file operations**: create, read, write, delete files ✅
- **Directory support** for organizing trading algorithms ✅
- **Commands**: `ls`, `cat`, `mkdir`, `rm`, `cp`, `mv` ✅

#### 2. Enhanced Memory Management ✅ **COMPLETED**
- **Virtual memory** with paging support ✅
- **Memory protection** between processes ✅
- **Improved heap allocator** with best-fit algorithm and debugging ✅
- **Memory debugging** tools and statistics ✅
- **Commands**: `memstats`, `memleak`, `memcheck`, `pgstats`

#### 3. Process Management & Scheduling
- **Multi-tasking support** for concurrent trading algorithms
- **Process creation/termination** (`fork`, `exec`, `kill`)
- **Priority-based scheduler** for real-time trading
- **Inter-process communication** (pipes, shared memory)

### Phase 2: System Services (Important)

#### 4. Network Stack
- **TCP/IP implementation** for market data feeds
- **Ethernet driver** for network connectivity
- **Socket API** for network programming
- **DHCP client** for automatic IP configuration

#### 5. Timer & Clock Services
- **High-precision timers** for microsecond trading
- **Real-time clock (RTC)** support
- **System uptime** and performance counters
- **Scheduling based on time events**

#### 6. Device Drivers
- **Disk/Storage drivers** (IDE/SATA)
- **Serial port communication** for external devices
- **USB support** for peripherals
- **Sound card** for alerts/notifications

### Phase 3: Trading-Specific Features (Specialized)

#### 7. Trading Engine Core
- **Market data structures** (orders, trades, positions)
- **Order management system** with validation
- **Risk management** algorithms
- **Portfolio tracking** and P&L calculation

#### 8. Real-Time Data Processing
- **Lock-free data structures** for high-frequency trading
- **Event-driven architecture** for market events
- **Low-latency message queues**
- **Market data parsers** (FIX protocol, etc.)

#### 9. Strategy Framework
- **Plugin system** for trading strategies
- **Backtesting engine** with historical data
- **Performance analytics** and reporting
- **Configuration management** for strategies

### Phase 4: Development Tools (Quality of Life)

#### 10. Debugging & Monitoring
- **Built-in debugger** with breakpoints
- **System profiler** for performance analysis
- **Log management** system
- **Resource monitoring** (CPU, memory, network)

#### 11. Development Environment
- **Text editor** within the OS
- **Compiler integration** for C/Assembly
- **Version control** (basic Git-like system)
- **Package manager** for libraries

### Implementation Priority

**Next 3 Recommended Features:**
1. **Network Stack** - Critical for receiving real-time market data feeds
2. **Timer & Clock Services** - High-precision timers for trading
3. **Enhanced File Writing** - Add text editor and file modification capabilities

**Example Future Commands:**
```bash
$ ls                    # List files and directories
$ mkdir strategies      # Create directory for trading algorithms
$ cat strategy.txt      # Display file contents
$ edit myalgo.c        # Built-in text editor
$ compile myalgo.c     # Compile trading strategy
$ run myalgo           # Execute trading algorithm
$ netstat              # Show network connections
$ top                  # Show running processes
$ df                   # Show disk usage
```

## License

This project is licensed under the same terms as the original TradeKernel project.

## Contributing

Feel free to extend this OS with additional features. The modular architecture makes it easy to add new subsystems while maintaining clean separation of concerns.