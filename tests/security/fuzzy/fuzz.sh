#!/bin/bash
cpus=$(nproc)

# First arg is binary to be fuzzed path
# When the value is "clean" the fuzzers are killed
bin_path="$1"

# Second arg can be "--one-job" to run one fuzzer instance

if [ "$1" != "clean" ]; then
        echo "Cleaning afl-o directory"
        rm -rf afl-o/*

        for (( cpu=0; cpu<cpus; cpu++ ))
        do
                if [ $cpu = "0" ]; then
                        # Master
                        echo "Starting master$cpu"
                        screen -S master -d -m
                        command="afl-fuzz -t 400 -m 512 -M master -i afl-i -o afl-o ${bin_path}"
                        screen -S master -X stuff "${command}\n"
                else
                        if [ "$2" != "--one-job" ]; then
                            echo "Starting slave$cpu"
                            screen -S slave$cpu -d -m
                            command="afl-fuzz -t 400 -m 512 -S slave$cpu -i afl-i -o afl-o ${bin_path}"
                            screen -S slave$cpu -X stuff "${command}\n"
                        fi
                fi
        done
fi

if [ "$1" = "clean" ]; then
        killall -s SIGINT afl-fuzz
        sleep 3
        screen -X -S master kill

        for (( cpu=1; cpu<cpus; cpu++ ))
        do
            screen -X -S slave${cpu} kill
        done
fi

