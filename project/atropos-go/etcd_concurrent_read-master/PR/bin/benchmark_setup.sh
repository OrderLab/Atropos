#!/bin/bash

# Specify the etcd benchmark parameters
NUM_WRITES=20
NUM_EXPENSIVE_READS=1
NUM_CHEAP_READS=100
NUM_KEYS=100000
KEY_SIZE=256
VALUE_SIZE=1024
ENDPOINT="http://localhost:2379"

# Specify the key-value file
KV_FILE="kv.txt"

# # Read each line from the key-value file and perform PUT operations in etcd
# echo "Starting put keys..."
# while IFS= read -r line; do
#     # Split the line into key and value
#     IFS='=' read -r key value <<< "$line"

#     # Perform the PUT operation in etcd
#     ./bin/etcdctl put "$key" "$value" > ./output/kv_setup.log
# done < "$KV_FILE"

# Perform the read benchmark in the background and direct output to a file
echo "Starting the expensive read benchmark..."
./bin/benchmark range ! \\0 \
    --endpoints=$ENDPOINT \
    --rate=1 \
    --total=10 \
    > ./output/expensive_read.log 2>&1 &

# Perform the read benchmark in the background and direct output to a file
echo "Starting the cheap read benchmark..."
./bin/benchmark range k\
    --endpoints=$ENDPOINT \
    --rate=$NUM_CHEAP_READS \
    --total=3000 \
    > ./output/cheap_read.log 2>&1 &

# Perform the write benchmark in the background and direct output to a file
echo "Starting the cheap write benchmark..."
./bin/benchmark put \
    --endpoints=$ENDPOINT \
    --rate=$NUM_WRITES \
    --total=1000 \
    > ./output/cheap_write.log 2>&1 &
