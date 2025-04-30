#!/bin/bash

TRACER="Tracers/DataTracer/obj-intel64/champsim_tracer.so"

CONVENTIONAL_PROGRAM="ConventionalCacheWithData/bin/champsim"
REUSE_PROGRAM="ReuseCacheWithData/bin/champsim"

MODELS_DIR="ONNX-Runtime-Inference/data/models"

OUTPUT_DIR="MLOutputs"
mkdir -p "$OUTPUT_DIR"
cd "$OUTPUT_DIR" || exit 1
mkdir -p conventional_output
mkdir -p reuse_cache_output
cd .. || exit 1

NUM_JOBS=4

job_count=0

generate_trace_and_run() {
    local model=$1
    local model_name=$(basename "$model" .onnx)

    echo "Generating trace for $model_name"
    pin -t "$TRACER" -o "$model_name.$type.champsim.trace" -- build/src/inference --use_cpu $model > /dev/null 2>&1

    echo "Running simulation for $model_name"

    $CONVENTIONAL_PROGRAM --warmup_instructions 50000000 --simulation_instructions 450000000 --uncompressed_trace -traces "$model_name.$type.champsim.trace" > "$OUTPUT_DIR/conventional_output/${model_name}.out" 2>&1 &
    $REUSE_PROGRAM --warmup_instructions 50000000 --simulation_instructions 450000000 --uncompressed_trace -traces "$model_name.$type.champsim.trace" > "$OUTPUT_DIR/reuse_cache_output/${model_name}.out" 2>&1

    rm "$model_name.$type.champsim.trace"
    
    echo "Simulation for $model_name completed"
}

#Loop through model
for model in $MODELS_DIR/*.onnx; do
    model_name=$(basename "$model" .onnx)

    generate_trace_and_run "$model" &
    ((job_count++))

    # Limit parallel jobs
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi

done
