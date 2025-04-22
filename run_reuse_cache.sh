#!/bin/bash

export PATH=~/BTP/gcc7.5/gcc-7.5.0/bin/bin:$PATH
export CC=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/gcc
export CXX=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/g++

make clean
make

PROGRAM=bin/champsim
mkdir -p reuse_cache_output

# Set how many jobs you want to run in parallel
NUM_JOBS=$(nproc)  # or set manually, e.g., NUM_JOBS=8
job_count=0

# Function to run the simulation
run_simulation() {
    local TRACE="$1"
    local BASENAME=$(basename "$TRACE")
    local NUMBER=$(echo "$BASENAME" | sed -n 's/.*-\([0-9]\+\)B\..*/\1/p')
    
    local BILLION=1000000000
    local MILLION=1000000
    local SIMULATION=$(echo "$NUMBER * $BILLION" | bc)
    local WARMUP=$(echo "$NUMBER * $MILLION" | bc)
    
    local OUTPUT_FILE="reuse_cache_output/${BASENAME%.xz}.out"

    echo "Running trace $BASENAME"
    echo "$PROGRAM --warmup_instructions $WARMUP --simulation_instructions $SIMULATION -traces $TRACE > $OUTPUT_FILE"
    $PROGRAM --warmup_instructions "$WARMUP" --simulation_instructions "$SIMULATION" -traces "$TRACE" > "$OUTPUT_FILE" 2>&1
}

# Loop and run simulations in parallel
for TRACE in traces/*.xz; do
    run_simulation "$TRACE" &
    ((job_count++))

    # Wait for jobs if limit reached
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi
done

# Final wait for any remaining jobs
wait

echo "✅ All simulations completed."
