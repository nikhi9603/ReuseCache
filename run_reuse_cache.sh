#!/bin/bash

export PATH=~/BTP/gcc7.5/gcc-7.5.0/bin/bin:$PATH
export CC=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/gcc
export CXX=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/g++

make clean

make

# Path to your program executable
PROGRAM=bin/champsim

# Create an output directory if not exists
mkdir -p reuse_cache_output

# Loop through all traces in the traces/ folder
for TRACE in traces/*.xz; do
    # Get the base filename
    BASENAME=$(basename "$TRACE")
    
    # Extract the number after the dash and before the 'B' (e.g., 41 from 400.perlbench-41B.champsimtrace.xz)
    NUMBER=$(echo "$BASENAME" | sed -n 's/.*-\([0-9]\+\)B\..*/\1/p')

    BILLION=1000000000
    MILLION=1000000
    SIMULATION=$(echo "$NUMBER * $BILLION" | bc)
    WARMUP=$(echo "$NUMBER * $MILLION" | bc)


    # Construct the output filename
    OUTPUT_FILE="reuse_cache_output/${BASENAME%.xz}.out"

    echo "Running trace $BASENAME"
    echo "$PROGRAM --warmup_instructions $WARMUP --simulation_instructions $SIMULATION -traces $TRACE > $OUTPUT_FILE"

    # Run the program with the number and trace file as arguments, save output
    $PROGRAM --warmup_instructions "$WARMUP" --simulation_instructions "$SIMULATION" -traces "$TRACE" > "$OUTPUT_FILE" 2>&1
done
