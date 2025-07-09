# TradeKernel
**Bare-Metal Real-Time OS for Ultra-Low-Latency Trading**

## Overview
TradeKernel is a deterministic operating system engineered for high-frequency trading (HFT), written in C++ and x86_64 Assembly. It eliminates traditional OS jitter through a custom tickless scheduler, kernel-bypass networking, and pre-allocated memory pools to achieve sub-microsecond latency.

## Architecture

### Core Components
- **Kernel**: 64-bit x86 kernel with optimized entry points
- **Memory Manager**: Lock-free NUMA-aware memory pools
- **Scheduler**: Tickless priority-based task scheduler (<500ns context switches)
- **Networking**: Zero-copy packet processing with ring buffers
- **Drivers**: Direct hardware access for NICs and storage

### Key Features
* **Tickless Scheduler**: CPU isolation and priority-based task dispatch with <500ns context switches
* **Zero-Copy Networking**: Direct NIC access via kernel-bypass (UDP multicast optimized)
* **Deterministic Memory**: Lock-free memory pools with NUMA awareness
* **Hardware Profiling**: Cycle-accurate metrics using RDTSCP and PMU counters
* **Cache Optimization**: Critical data structures are cache-line aligned
* **Real-time Guarantees**: Predictable execution times for trading algorithms

## Build & Deploy

### Requirements
* **Hardware**: x86_64 with Intel/AMD CPUs (NUMA-aware) or ARMv8+
* **Toolchain**: Clang/LLVM with Link-Time Optimization
* **Dependencies**: NASM (assembler), QEMU (testing), GNU Make

### Installation (macOS)
```bash
# Install dependencies
make install-deps

# Build the kernel
make clean && make all

# Run in QEMU with hardware acceleration
make run
```

### Manual Build
```bash
git clone https://github.com/yourusername/TradeKernel.git  
cd TradeKernel  
make clean && make all
make run  # QEMU-KVM emulation
```

## Performance Targets

| Metric | Target | Implementation |
|--------|--------|----------------|
| Interrupt Latency | < 100ns | Direct interrupt handlers |
| NIC-to-UserSpace | < 300ns | Zero-copy ring buffers |
| Memory Access | 40ns | L1 cache optimization |
| Context Switch | < 500ns | Minimal register state |
| Packet Processing | < 1μs | Kernel-bypass networking |

## Project Structure

```
TradeKernel/
├── boot/                 # Bootloader (16/32/64-bit transition)
│   └── boot.asm         # BIOS bootloader with long mode setup
├── src/
│   ├── kernel/          # Core kernel implementation
│   │   ├── entry.asm    # 64-bit entry point and context switching
│   │   └── main.cpp     # Kernel initialization and main loop
│   ├── memory/          # Memory management subsystem
│   │   └── memory_manager.cpp  # NUMA-aware lock-free allocators
│   ├── scheduler/       # Task scheduling
│   │   └── tickless_scheduler.cpp  # Priority-based scheduler
│   ├── drivers/         # Hardware drivers
│   └── networking/      # Network stack
├── include/             # Header files
│   ├── types.h         # Core types and performance macros
│   ├── memory.h        # Memory management interfaces
│   ├── scheduler.h     # Scheduler interfaces
│   └── networking.h    # Networking interfaces
├── tools/              # Development tools
├── docs/               # Documentation
└── Makefile           # Build system with optimizations
```

## Usage Example

```cpp
// Create high-priority market data task
u32 market_task = g_scheduler->create_task(
    Priority::CRITICAL,
    market_data_processor,
    nullptr,
    16384  // 16KB stack
);

// Process packets with zero-copy
NetworkPacket packet;
if (interface->receive_packet(packet)) {
    auto* udp_header = packet.get_udp_header();
    auto* payload = packet.get_payload();
    
    // Process market data with sub-microsecond latency
    process_market_update(payload, packet.get_payload_size());
}
```

## Optimization Features

### Compiler Optimizations
- **-O3**: Maximum optimization level
- **-march=native**: CPU-specific optimizations
- **-flto**: Link-time optimization
- **-ffast-math**: Aggressive floating-point optimizations
- **Cache alignment**: Critical structures aligned to 64-byte cache lines

### Runtime Optimizations
- **Lock-free data structures**: Atomic operations instead of mutexes
- **NUMA awareness**: Memory allocation on local nodes
- **CPU affinity**: Tasks pinned to specific cores
- **Ring buffers**: Zero-copy packet processing
- **Prefetching**: Software prefetching for predictable access patterns

## Benchmarking

```bash
# Run performance benchmarks
make benchmark

# Debug with performance monitoring
make debug

# Performance analysis with QEMU
make perf
```

## Development

### Adding New Components
1. Create header in `include/`
2. Implement in appropriate `src/` subdirectory
3. Update `Makefile` with new source files
4. Add to kernel initialization in `main.cpp`

### Contributing
- **Drivers**: NVMe, FPGA co-processors, InfiniBand
- **Benchmarks**: Against Linux RT, Xenomai, seL4
- **Optimizations**: SIMD operations, hardware timestamping
- **Guidelines**: See `CONTRIBUTING.md`

## Resources
- [TradeKernel Architecture](docs/architecture.md)
- [Performance Tuning Guide](docs/performance.md)
- [Network Programming](docs/networking.md)
- [Memory Management](docs/memory.md)
- [Real-time Programming](docs/realtime.md)

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

**Warning**: This is a bare-metal operating system intended for dedicated trading hardware. Do not run on production systems without proper isolation.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
