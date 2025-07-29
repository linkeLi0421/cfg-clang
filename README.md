# Source Level Control flow Graph with Data Dependency Analysis

## Overview
This tool analyzes C/C++ source code to extract control flow graphs (CFG) with enhanced data dependency analysis including def-use chains.

## Build Instructions
```bash
cd cfg-clang
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/llvm/build ..
make
```

## Usage
```bash
./cfg-clang -p ./compile_commands.json {target_file}
```

## Features
- **Control Flow Graph**: Extracts basic blocks with predecessor/successor relationships
- **Line Number Mapping**: Maps each basic block to source line ranges
- **Def-Use Chain Analysis**: Tracks variable definitions and uses across the CFG
- **Data Dependencies**: Identifies variable assignments and usages with context

## Output Format
The tool outputs enhanced CFG information including:
- Basic block identifiers (B0, B1, etc.)
- Predecessor and successor relationships
- Line number ranges for each block
- Def-use chain information showing:
  - Variable definitions (DEF) with line numbers
  - Variable usages (USE) with line numbers
  - Context (variable_declaration, assignment, variable_use)

## Example Output
```
Function signature: int example_function(int x)
Defined in file: test.c at line 10

[B0]
  Preds (0): 
  Succs (1): B1
  Def-Use Chain:
    DEF x at line 10 (variable_declaration)
    USE x used at line 12 (variable_use)
from line 10 to line 15
```
