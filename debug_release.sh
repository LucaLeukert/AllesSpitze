#!/bin/bash
# Debug script for Release build segfaults
# Usage: ./debug_release.sh

echo "=== AllesSpitzeQt Release Debug Helper ==="
echo ""

# Check if running on remote host
if [ ! -f "/proc/device-tree/model" ]; then
    echo "⚠️  This script should be run on the Raspberry Pi/ARM target"
    exit 1
fi

echo "1. Building with debug symbols..."
cd cmake-build-release-remote-host
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "2. Running with GDB to catch segfault..."
echo "   (Type 'run' and press Enter, then 'bt' after crash for backtrace)"
echo ""

# Set QML debugging environment
export QT_LOGGING_RULES="*.debug=true"
export QT_QPA_PLATFORM=eglfs
export QML_DISABLE_DISK_CACHE=1

# Run with gdb
gdb -ex "set pagination off" \
    -ex "handle SIGSEGV stop print" \
    -ex "run" \
    -ex "bt full" \
    -ex "info registers" \
    -ex "quit" \
    ./AllesSpitzeQt 2>&1 | tee crash_log.txt

echo ""
echo "✅ Crash log saved to: $(pwd)/crash_log.txt"
echo ""
echo "To analyze further, look for:"
echo "  - 'Program received signal SIGSEGV' - shows crash location"
echo "  - Stack frames with '???' - missing debug symbols"
echo "  - Last function called before crash"

