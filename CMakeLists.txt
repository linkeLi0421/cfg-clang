# Set the minimum required version of CMake.
cmake_minimum_required(VERSION 3.14)

# Define the project name.
project(MyClangTool)

# Set the C++ standard to C++17, which is required by modern LLVM.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the LLVM and Clang packages.
# THIS IS THE CRUCIAL PART THAT USES CMAKE_PREFIX_PATH.
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Add the necessary include directories from LLVM and Clang.
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

# Add compiler definitions required by LLVM.
add_definitions(${LLVM_DEFINITIONS})

# Define the executable target for your project.
add_executable(cfg-clang src/main.cpp)

# Link your executable against the required Clang and LLVM libraries.
target_link_libraries(cfg-clang PRIVATE clang-cpp)