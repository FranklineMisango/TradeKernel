#!/bin/bash

# TradeKernel OS Build and Run Script

echo "=== TradeKernel OS Build System ==="
echo

# Check if required tools are installed
check_dependencies() {
    echo "Checking dependencies..."
    
    if ! command -v gcc &> /dev/null; then
        echo "Error: gcc not found. Please install build-essential."
        echo "Run: sudo apt-get install build-essential"
        exit 1
    fi
    
    if ! command -v nasm &> /dev/null; then
        echo "Error: nasm not found. Please install nasm."
        echo "Run: sudo apt-get install nasm"
        exit 1
    fi
    
    if ! command -v qemu-system-i386 &> /dev/null; then
        echo "Error: qemu-system-i386 not found. Please install qemu."
        echo "Run: sudo apt-get install qemu-system-x86"
        exit 1
    fi
    
    echo "All dependencies found!"
    echo
}

# Build the OS
build_os() {
    echo "Building TradeKernel OS..."
    make clean
    make all
    
    if [ $? -eq 0 ]; then
        echo "Build successful!"
        echo
    else
        echo "Build failed!"
        exit 1
    fi
}

# Run the OS in QEMU
run_os() {
    echo "Starting TradeKernel OS in QEMU..."
    echo "Press Ctrl+Alt+G to release mouse from QEMU"
    echo "Press Ctrl+Alt+2 for QEMU monitor, Ctrl+Alt+1 to return to OS"
    echo "Press Ctrl+C in this terminal to quit QEMU"
    echo
    make run
}

# Main execution
check_dependencies
build_os
run_os