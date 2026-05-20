#!/bin/bash

NORMAL_TRACER="../Tracers/ConventionalTracer/obj-intel64/champsim_tracer.so"
DATA_TRACER="../Tracers/DataTracer/obj-intel64/champsim_tracer.so"

CONVENTIONAL_DATA_PROGRAM="../ConventionalCacheWithData/bin/champsim"
REUSE_DATA_PROGRAM="../ReuseCacheWithData/bin/champsim"

CONVENTIONAL_NORMAL_PROGRAM="../ConventionalCache/bin/champsim"
REUSE_NORMAL_PROGRAM="../ReuseCache/bin/champsim"

ONNX_RUNTIME_DIR="ONNX-Runtime-Inference"
MODELS_DIR="data/models_list/"

NORMAL_OUTPUT_DIR="MLOutputs_Normal"
mkdir -p "$NORMAL_OUTPUT_DIR"
cd "$NORMAL_OUTPUT_DIR" || exit 1
mkdir -p conventional_output
mkdir -p reuse_cache_output

# mkdir -p conventional_output_40
# mkdir -p reuse_cache_output_40

# cd .. || exit 1

# DATA_OUTPUT_DIR="MLOutputs_Data"
# mkdir -p "$DATA_OUTPUT_DIR"
# cd "$DATA_OUTPUT_DIR" || exit 1
# mkdir -p conventional_output
# mkdir -p reuse_cache_output

cd ../ || exit 1

NUM_JOBS=1

job_count=0

if [ $# -eq 0 ]; then
models=(
        # "densenet-12.onnx"
        # "densenet-12-int8.onnx"
        # "mobilenetv2-12.onnx"
        # "mobilenetv2-12-int8.onnx"
        # "resnet18-v1-7.onnx"
        # "resnet50-v1-12.onnx"
        # "shufflenet-v2-12.onnx"
        # "shufflenet-v2-12-int8.onnx"
        # "squeezenet1.1-7.onnx"
        )
else
    models=("$@")
fi

# for f in "$MODELS_DIR"*.onnx; do
#     models+=("$f")
# done

generate_trace_and_run() {
    local model=$1
    local model_name=$(basename "$model" .onnx)

    # echo "Generating trace for $model_name"
    # ../Tools/pin-3.20-98437-gf02b61307-gcc-linux/pin -t "$NORMAL_TRACER" -o "$model_name.champsim.normal.trace" -- build/src/inference --use_cpu $model > /dev/null 2>&1

    # not tracing into output data trace file and just doing marker mode/
    # ../Tools/pin-3.20-98437-gf02b61307-gcc-linux/pin -t "$DATA_TRACER" -w "1" -m "1" -k "2500000000" -o "$model_name.champsim.data.trace" -- build/src/inference --use_cpu $model > "$model_name.marker_mode.debugTrace.txt"
    
    # not tracing into output data trace file and no marker mode, just based on -s -t
    # ../Tools/pin-3.20-98437-gf02b61307-gcc-linux/pin -t "$DATA_TRACER" -w "0" -s "100" -t "100" -- build/src/inference --use_cpu $model > "$model_name.non-marker.debugTrace.txt"

    # tracing into output data trace file based on your s and t values from above without tracing
    # ../Tools/pin-3.20-98437-gf02b61307-gcc-linux/pin -t "$DATA_TRACER" -o "$model_name.champsim.data.trace" -s "100" -t "100" -- build/src/inference --use_cpu $model > "$model_name.trace-writing.debugTrace.txt"

    echo "Running simulation for $model_name"
    # $CONVENTIONAL_NORMAL_PROGRAM --warmup_instructions 80000000 --simulation_instructions 200000000 --uncompressed_trace -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/conventional_output/${model_name}.out" 2>&1
    # $REUSE_NORMAL_PROGRAM --warmup_instructions 80000000 --simulation_instructions 200000000 --uncompressed_trace --reuse_cache_llc -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output/${model_name}.out" 2>&1

    # $CONVENTIONAL_NORMAL_PROGRAM --warmup_instructions 30000000 --simulation_instructions 400000000 --uncompressed_trace -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/conventional_output_40/${model_name}.out" 2>&1
    # $REUSE_NORMAL_PROGRAM --warmup_instructions 30000000 --simulation_instructions 400000000 --uncompressed_trace --reuse_cache_llc -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output_40/${model_name}.out" 2>&1

    # $CONVENTIONAL_DATA_PROGRAM --warmup_instructions 10000000 --simulation_instructions 200000000 --uncompressed_trace -traces "$model_name.champsim.data.trace" > "../$NORMAL_OUTPUT_DIR/conventional_output/${model_name}.out" 

    LOG_FILE="$model_name.marker_mode.debugTrace.txt"

    echo "Parsing marker log for $model_name..."

    WARMUP_INSTR=$(grep "ChampSim warmup" "$LOG_FILE" | awk '{print $3}')
    SIMULATION_INSTR=$(grep "ChampSim simulate" "$LOG_FILE" | awk '{print $3}')

    if [ -z "$WARMUP_INSTR" ] || [ -z "$SIMULATION_INSTR" ]; then
    echo "Error: Failed to extract warmup/sim values for $model_name"
    exit 1
    fi

    echo "Warmup: $WARMUP_INSTR"
    echo "Simulate: $SIMULATION_INSTR"

    gdb -ex run -ex bt -ex quit --args $CONVENTIONAL_DATA_PROGRAM \
    --warmup_instructions "$WARMUP_INSTR" \
    --simulation_instructions "$SIMULATION_INSTR" \
    --uncompressed_trace \
    -traces "$model_name.champsim.data.trace" > "$model_name-conventional-output.txt"

    
    # gdb -ex run -ex bt -ex quit --args $REUSE_DATA_PROGRAM \
    # --warmup_instructions "$WARMUP_INSTR" \
    # --simulation_instructions "$SIMULATION_INSTR" \
    # --uncompressed_trace \
    # -traces "$model_name.champsim.data.trace" > "$model_name-reuse-output.txt"
    
    # $REUSE_DATA_PROGRAM --warmup_instructions 50000000 --simulation_instructions 400000000 --uncompressed_trace -traces "$model_name.champsim.data.trace" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output/${model_name}.out" 2>&1

    #rm "$model_name.champsim.normal.trace"
    # rm "$model_name.champsim.data.trace"
    
    echo "Simulation for $model_name completed"
}

#Loop through model
for model in "${models[@]}"; do
    model_name=$(basename "$model" .onnx)
    echo "$model_name"

    generate_trace_and_run "$model" &
    ((job_count++))

    # Limit parallel jobs
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi

done
