#!/bin/bash

# Specify the etcd benchmark parameters
NUM_WRITES=40
NUM_EXPENSIVE_READS=1
NUM_READS=100
NUM_KEYS=100000
KEY_SIZE=256
VALUE_SIZE=1024
ENDPOINT="http://localhost:2379"

# Please config and make dir before test
TTL=$1
OUTPUT_DIR=./output_cpu/${TTL}ms_cancel
# OUTPUT_DIR=./output_100k_mem_8G/baseline_no_expensive
mkdir $OUTPUT_DIR 

# export ETCDCTL_API=3

# # Check if the number of key-value pairs is provided as an argument
# if [[ $# -ne 1 ]]; then
#     echo "Usage: $0 <num_pairs>"
#     exit 1
# fi

# Extract the number of key-value pairs from the argument
# NUM_PAIRS=$1

# # Generate and write the random key-value pairs
# echo "Generating $NUM_PAIRS random key-value pairs..."
# for ((i=1; i<=NUM_PAIRS; i++)); do
#     sequential_number=$i
#     random_value=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 240 | head -n 1)
#     if [[ $((i % 5)) -eq 1 ]]; then
#         key="a${sequential_number}${random_value}"
#     else
#         key="${sequential_number}${random_value}"
#     fi    
#     value=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 1024 | head -n 1)
#     ./bin/etcdctl put "$key" "$value" > $OUTPUT_DIR/kv_setup.log
# done

# # Perform the read benchmark in the background and direct output to a file
# echo "Starting the expensive read benchmark..."
# ./bin/benchmark range 1 2 \
#     --endpoints=$ENDPOINT \
#     --rate=1 \
#     --total=11 \
#     > $OUTPUT_DIR/expensive_read.log 2>&1 &

go run ./cpu_study $TTL &
# go run ./cancel_study $TTL &

# Perform the read benchmark in the background and direct output to a file
echo "Starting the cheap read benchmark..."
./bin/benchmark range 14980 14981\
    --endpoints=$ENDPOINT \
    --rate=$NUM_READS \
    --total=1000 \
    > $OUTPUT_DIR/cheap_read.log 2>&1 &

# Perform the write benchmark in the background and direct output to a file
echo "Starting the cheap write benchmark..."
./bin/benchmark put \
    --endpoints=$ENDPOINT \
    --rate=$NUM_WRITES \
    --total=180 \
    > $OUTPUT_DIR/cheap_write.log 2>&1 &

# for ((i=1; i<10; i++)); do
#     j=$i+1
#     # Perform the read benchmark in the background and direct output to a file
#     ./bin/benchmark range $i $j \
#         --endpoints=$ENDPOINT \
#         --rate=1 \
#         --total=1 \
#         > $OUTPUT_DIR/range_${i}.log 2>&1 &
#     sleep 1
# done
