#!/bin/bash

make clean
make

all=0
fail=0

for filename in command_files/*.command; do
    #echo $filename
    test_num=`echo $filename | cut -d'.' -f1 | cut -d'/' -f2`
     bash ${filename} > ourout_files/${test_num}.YoursOut
done

for filename in output_files/*.out; do
    (( all++ ))
    test_num=`echo $filename | cut -d'.' -f1 | cut -d'/' -f2`
    diff_result=$(diff output_files/${test_num}.out ourout_files/${test_num}.YoursOut)
    if [ "$diff_result" != "" ]; then
        echo The test ${test_num} didnt pass
	    (( fail++ ))
    fi
done

for filename in long_tests/*.command; do
    #echo $filename
    test_num=`echo $filename | cut -d'.' -f1 | cut -d'/' -f2`
     bash ${filename} > ourout_long_files/${test_num}.YoursOut
done

for filename in long_tests/*.out; do
    (( all++ ))
    test_num=`echo $filename | cut -d'.' -f1 | cut -d'/' -f2`
    diff_result=$(diff long_tests/${test_num}.out ourout_long_files/${test_num}.YoursOut)
    if [ "$diff_result" != "" ]; then
        echo The test ${test_num} didnt pass
        (( fail++ ))
    fi
done

echo
echo "faild $fail out of $all "
echo Ran all tests.

# rm -f ourout_files/*

bash example1_command > example1.YoursOut
bash example2_command > example2.YoursOut
bash example3_command > example3.YoursOut

diff -q example1_output example1.YoursOut
diff -q example2_output example2.YoursOut
diff -q example3_output example3.YoursOut
