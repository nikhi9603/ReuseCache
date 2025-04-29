#!/bin/bash

CONVENTIONAL_TRACER="../Tracers/ConventionalTracer/obj-intel64/champsim_tracer.so"
DATA_TRACER="../Tracers/DataTracer/obj-intel64/champsim_tracer.so"

MODELS_DIR="data/models"

TRACES_DIR="../MLtraces"
mkdir -p $TRACES_DIR

CONVENTIONAL_TRACE_DIR="$TRACES_DIR/NormalTrace"
mkdir -p $CONVENTIONAL_TRACE_DIR

DATA_TRACE_DIR="$TRACES_DIR/DataTrace"
mkdir -p $DATA_TRACE_DIR

NUM_JOBS=2

job_count=0

download_trace() {
    local model=$1
    local tracer=$2
    local output_file=$3
    local output_file_path=$4

    echo "Creating trace for $model with $tracer"

    pin -t "$tracer" -o "$output_file" -- build/src/inference --use_cpu $model > /dev/null 2>&1
    xz -9 -T0 "$output_file" > /dev/null 2>&1
    mv "$output_file.xz" "$output_file_path/$output_file.xz"

}

#Loop through model
for model in $MODELS_DIR/*.onnx; do
    model_name=$(basename "$model" .onnx)

    download_trace "$model" "$CONVENTIONAL_TRACER" "$model_name.normal.trace" "$CONVENTIONAL_TRACE_DIR" &
    ((job_count++))

    download_trace "$model" "$DATA_TRACER" "$model_name.data.trace" "$DATA_TRACE_DIR" &
    ((job_count++))

    # Limit parallel jobs
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi

done
