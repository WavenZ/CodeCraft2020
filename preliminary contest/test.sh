#!/bin/bash

test_file[0]="test_data_56.txt"
test_file[1]="test_data_3738.txt"
test_file[2]="test_data_58284.txt"
test_file[3]="test_data_1004812.txt"

answer_file[0]="answer_56.txt"
answer_file[1]="answer_3738.txt"
answer_file[2]="answer_58284.txt"
answer_file[3]="answer_1004812.txt"

if [ "$2" = "DDEBUG" ]
then 
    g++ -O3 $1 -o test -lpthread -DDEBUG
else
    g++ -O3 $1 -o test -lpthread
fi

if [ ! -d "/data/" ]
then
    mkdir /data
fi

if [ ! -d "/projects/student/" ]
then 
    mkdir /projects
    mkdir /projects/student
fi


echo "------------------"

date > log.txt
old=1
cnt=0
total_cnt=0

for ((i=0;i<${#test_file[@]};i++))
do
    echo ${test_file[$i]}
    cp ${test_file[$i]} /data/test_data.txt
    time ./test
    if [ -f "/projects/student/result.txt" ]
    then
        diff /projects/student/result.txt "${answer_file[$i]}" >> log.txt
        len=$(sed -n '$=' log.txt)
        if [ $len -gt $old ]
        then
            echo "Fail!"
        else 
            echo "Pass!"
            let cnt+=1
        fi
        rm /projects/student/result.txt
    else
        echo "File not generated !"
    fi
        old=$len
    let total_cnt+=1
    echo "------------------"
done

echo "$cnt/$total_cnt Passed" | tee -a ./log.txt
    
