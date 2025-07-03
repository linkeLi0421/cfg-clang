# Source Level Control flow Graph

* cd cfg-clang
* mkdir build;
* cd build;
* cmake -DCMAKE_PREFIX_PATH=/llvm/build  ..;

* build/cfg-clang -p ./compile_commands.json {target_file}
