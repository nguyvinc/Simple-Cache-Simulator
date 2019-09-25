# Simple-Cache-Simulator
Simple Hardware Cache Simulator written in C

Two files are needed to run the simulation: a program trace file and a cache configuration file.

To produce a program trace file with valgrind, use the command
valgrind --log-fd=1 --log-file=./tracefile.txt --tool=lackey --trace-mem=yes name_of_executable_to_trace

The cache configuration file must have 7 lines of settings. (The following is an example)
8   <-- number of sets in cache   (will be a non-negative power of 2)
16  <-- block size in bytes       (will be a non-negative power of 2)
3   <-- level of associativity    (number of blocks per set)
1   <-- replacement policy        (will be 0 for random replacement, 1 for LRU)
1   <-- write policy              (will be 0 for write-through, 1 for write-back)
13  <-- number of cycles required to write or read a block from the cache
230 <-- number of cycles required to write or read a block from main memory
