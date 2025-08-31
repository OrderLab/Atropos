#!/bin/bash

# Check if the number of key-value pairs is provided as an argument
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <num_pairs>"
    exit 1
fi

# Extract the number of key-value pairs from the argument
NUM_PAIRS=$1

# Specify the output file
OUTPUT_FILE="kv.txt"

# Generate and write the random key-value pairs to the file
echo "Generating $NUM_PAIRS random key-value pairs..."
for ((i=1; i<=NUM_PAIRS; i++)); do
    key=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 256 | head -n 1)
    value=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 1024 | head -n 1)
    echo "$key=$value" >> "$OUTPUT_FILE"
done

echo "Random key-value pairs generated and written to $OUTPUT_FILE."
