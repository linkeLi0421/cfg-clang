#!/bin/bash

# Test script for enhanced cfg-clang with def-use chain analysis

echo "=== Building Enhanced cfg-clang Tool ==="
cd cfg-clang
mkdir -p build
cd build

# Configure and build
echo "Configuring with CMake..."
cmake -DCMAKE_PREFIX_PATH=/llvm/build .. || {
    echo "Error: LLVM build path not found. Please adjust CMAKE_PREFIX_PATH"
    echo "Usage: cmake -DCMAKE_PREFIX_PATH=/path/to/llvm/build .."
    exit 1
}

echo "Building..."
make

if [ $? -ne 0 ]; then
    echo "Build failed. Please ensure LLVM and Clang development packages are installed."
    exit 1
fi

echo "=== Build Complete ==="
echo

# Test with example file
cd ..
echo "=== Testing Enhanced CFG Analysis ==="

# Create a simple compile_commands.json for testing
cat > compile_commands.json << EOF
[
    {
        "directory": "$(pwd)",
        "file": "test_example.c",
        "arguments": ["gcc", "-c", "test_example.c", "-o", "test_example.o"]
    }
]
EOF

echo "Running enhanced CFG analysis on test_example.c..."
echo
./build/cfg-clang -p compile_commands.json test_example.c

echo "=== Test Complete ==="
echo
echo "To analyze your own files:"
echo "  1. Create compile_commands.json for your project"
echo "  2. Run: ./build/cfg-clang -p compile_commands.json your_source_file.c"