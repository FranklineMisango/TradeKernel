# TradeKernel Makefile
# Ultra-Low Latency Trading OS Build System

# Build configuration
ARCH := x86_64
BUILD_DIR := build
BOOT_DIR := boot
SRC_DIR := src
INCLUDE_DIR := include

# Compilers and tools
ASM := nasm
CXX := clang++
LD := ld
OBJCOPY := objcopy

# Assembly flags
ASMFLAGS := -f elf64

# C++ compiler flags for maximum performance
CXXFLAGS := -std=c++20 \
           -target x86_64-unknown-none-elf \
           -ffreestanding \
           -fno-exceptions \
           -fno-rtti \
           -fno-stack-protector \
           -fno-builtin \
           -nostdlib \
           -nostdinc++ \
           -mno-red-zone \
           -mcmodel=kernel \
           -O3 \
           -march=native \
           -mtune=native \
           -flto \
           -ffast-math \
           -funroll-loops \
           -fomit-frame-pointer \
           -finline-functions \
           -Wall \
           -Wextra \
           -Werror \
           -I$(INCLUDE_DIR)

# Linker flags
LDFLAGS := -T linker.ld \
          -nostdlib \
          -static \
          -n

# Source files
BOOT_SOURCES := $(BOOT_DIR)/boot.asm
KERNEL_ASM_SOURCES := $(SRC_DIR)/kernel/entry.asm
KERNEL_CXX_SOURCES := $(SRC_DIR)/kernel/main.cpp \
                     $(SRC_DIR)/memory/memory_manager.cpp \
                     $(SRC_DIR)/scheduler/tickless_scheduler.cpp

# Object files
BOOT_OBJECTS := $(BUILD_DIR)/boot.o
KERNEL_ASM_OBJECTS := $(BUILD_DIR)/entry.o
KERNEL_CXX_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(KERNEL_CXX_SOURCES))

ALL_OBJECTS := $(KERNEL_ASM_OBJECTS) $(KERNEL_CXX_OBJECTS)

# Targets
.PHONY: all clean kernel bootloader iso run debug

all: kernel bootloader

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/kernel
	mkdir -p $(BUILD_DIR)/memory  
	mkdir -p $(BUILD_DIR)/scheduler
	mkdir -p $(BUILD_DIR)/drivers
	mkdir -p $(BUILD_DIR)/networking

# Bootloader
bootloader: $(BUILD_DIR) $(BOOT_OBJECTS)

$(BUILD_DIR)/boot.o: $(BOOT_SOURCES)
	$(ASM) $(ASMFLAGS) -o $@ $<

$(BUILD_DIR)/boot.bin: $(BUILD_DIR)/boot.o
	$(LD) -Ttext 0x7C00 -o $@ $< --oformat binary

# Kernel
kernel: $(BUILD_DIR) $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(ALL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)

# Assembly sources
$(BUILD_DIR)/entry.o: $(SRC_DIR)/kernel/entry.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

# C++ sources
$(BUILD_DIR)/kernel/%.o: $(SRC_DIR)/kernel/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/memory/%.o: $(SRC_DIR)/memory/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/scheduler/%.o: $(SRC_DIR)/scheduler/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Create ISO image for QEMU
iso: $(BUILD_DIR)/tradekernel.iso

$(BUILD_DIR)/tradekernel.iso: bootloader kernel
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/iso/boot/
	echo 'menuentry "TradeKernel" {' > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.bin' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '}' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD_DIR)/iso

# Run in QEMU with optimizations for low latency
run: $(BUILD_DIR)/kernel.bin
	qemu-system-x86_64 \
		-kernel $(BUILD_DIR)/kernel.bin \
		-cpu host \
		-smp 4 \
		-m 512M \
		-no-reboot \
		-no-shutdown \
		-serial stdio \
		-enable-kvm

# Debug version
debug: CXXFLAGS += -g -DDEBUG
debug: kernel
	qemu-system-x86_64 \
		-kernel $(BUILD_DIR)/kernel.bin \
		-cpu host \
		-smp 4 \
		-m 512M \
		-no-reboot \
		-no-shutdown \
		-serial stdio \
		-s -S

# Performance analysis
perf: kernel
	qemu-system-x86_64 \
		-kernel $(BUILD_DIR)/kernel.bin \
		-cpu host \
		-smp 4 \
		-m 512M \
		-no-reboot \
		-no-shutdown \
		-serial stdio \
		-enable-kvm \
		-monitor unix:qemu-monitor-socket,server,nowait

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f tradekernel.iso

# Show build information
info:
	@echo "TradeKernel Build Configuration:"
	@echo "Architecture: $(ARCH)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "C++ Compiler: $(CXX)"
	@echo "Assembler: $(ASM)"
	@echo "Linker: $(LD)"
	@echo ""
	@echo "Optimization Flags:"
	@echo "  -O3: Maximum optimization"
	@echo "  -march=native: Optimize for current CPU"
	@echo "  -flto: Link-time optimization"
	@echo "  -ffast-math: Aggressive math optimizations"
	@echo ""
	@echo "Source Files:"
	@echo "  Kernel ASM: $(KERNEL_ASM_SOURCES)"
	@echo "  Kernel C++: $(KERNEL_CXX_SOURCES)"

# Install prerequisites (macOS)
install-deps:
	@echo "Installing build dependencies for macOS..."
	brew install nasm
	brew install qemu
	brew install llvm
	@echo "Dependencies installed!"

# Benchmark the kernel
benchmark: kernel
	@echo "Running TradeKernel benchmarks..."
	qemu-system-x86_64 \
		-kernel $(BUILD_DIR)/kernel.bin \
		-cpu host \
		-smp 4 \
		-m 512M \
		-no-reboot \
		-no-shutdown \
		-serial stdio \
		-enable-kvm \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0