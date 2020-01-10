#!/bin/bash
cpus=$(nproc)
echo core >/proc/sys/kernel/core_pattern

if [ "$1" != "clean" ]; then
        for (( cpu=0; cpu<cpus; cpu++ ))
        do
                if [ $cpu = "0" ]; then
                        # Master
                        echo "Starting master$cpu"
                        screen -S master -d -m
                        command="afl-fuzz -t 400 -m 512 -M master -i afl-i -o afl-o ./build/release/source/userspace/iotrace"
                        screen -S master -X stuff "${command}\n"
                else
                        echo "Starting slave$cpu"
                        screen -S slave$cpu -d -m
                        command="afl-fuzz -t 400 -m 512 -S slave$cpu -i afl-i -o afl-o ./build/release/source/userspace/iotrace"
                        screen -S slave$cpu -X stuff "${command}\n"
                fi
        done
fi

if [ "$1" = "clean" ]; then
        killall -s SIGINT afl-fuzz
        sleep 3
        killall screen
fi

