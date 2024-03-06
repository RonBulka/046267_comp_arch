#!/bin/bash

make clean
make

./cacheSim examples/example1_trace --mem-cyc 100 --bsize 3 --wr-alloc 1 --l1-size 4 --l1-assoc 1 --l1-cyc 1 --l2-size 6 --l2-assoc 0 --l2-cyc 5 > example1_ouroutput.out
./cacheSim examples/example2_trace --mem-cyc 50 --bsize 4 --wr-alloc 1 --l1-size 6 --l1-assoc 1 --l1-cyc 2 --l2-size 8 --l2-assoc 2 --l2-cyc 4 > example2_ouroutput.out
./cacheSim examples/example3_trace --mem-cyc 10 --bsize 2 --wr-alloc 1 --l1-size 4 --l1-assoc 1 --l1-cyc 1 --l2-size 4 --l2-assoc 2 --l2-cyc 5 > example3_ouroutput.out

# diff example1_ouroutput.out examples/example1_output
# diff example2_ouroutput.out examples/example2_output
# diff example3_ouroutput.out examples/example3_output

# rm -f example*_ouroutput.out