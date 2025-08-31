#!/bin/bash

# Specify the etcd benchmark parameters
KEY_SIZE=256
VALUE_SIZE=1024
ENDPOINT="http://localhost:2379"

# export ETCDCTL_API=3

# Check if the number of key-value pairs is provided as an argument
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <num_pairs>"
    exit 1
fi

NUM_PAIRS=$1

# Please config and make dir before test
OUTPUT_DIR=./output_$(NUM_PAIRS)/baseline
mkdir $OUTPUT_DIR 


# Generate and write the random key-value pairs
echo "Generating $NUM_PAIRS random key-value pairs..."
for ((i=1; i<=NUM_PAIRS; i++)); do
    sequential_number=$i
    random_value=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 240 | head -n 1)
    if [[ $((i % 5)) -eq 1 ]]; then
        key="a${sequential_number}${random_value}"
    else
        key="${sequential_number}${random_value}"
    fi    
    value=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 1024 | head -n 1)
    ./bin/etcdctl put "$key" "$value" > $OUTPUT_DIR/kv_setup.log
done
