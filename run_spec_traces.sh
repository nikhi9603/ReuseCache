#!/bin/bash

CONVENTIONAL_NORMAL_PROGRAM="ConventionalCache/bin/champsim"
# REUSE_NORMAL_PROGRAM="ReuseCache/bin/champsim"

TRACES_DIR="speccpu-traces"

NORMAL_OUTPUT_DIR="SpecOutputs_Normal"
mkdir -p "$NORMAL_OUTPUT_DIR"
cd "$NORMAL_OUTPUT_DIR" || exit 1
mkdir -p conventional_output
# mkdir -p reuse_cache_output

cd .. || exit 1

# cd "$ONNX_RUNTIME_DIR" || exit 1

NUM_JOBS=6

job_count=0

if [ $# -eq 0 ]; then
traces=(
  400.perlbench-50B.champsimtrace.xz    
  401.bzip2-7B.champsimtrace.xz  
  403.gcc-16B.champsimtrace.xz        
  403.gcc-48B.champsimtrace.xz           
  433.milc-127B.champsimtrace.xz         
  444.namd-44B.champsimtrace.xz          
  445.gobmk-36B.champsimtrace.xz        
  464.h264ref-64B.champsimtrace.xz       
  625.x264_s-39B.champsimtrace.xz 
  458.sjeng-31B.champsimtrace.xz
  445.gobmk-17B.champsimtrace.xz
  657.xz_s-56B.champsimtrace.xz       
)
else
    traces=("$@")
fi


# for f in "$TRACES_DIR"/*.onnx; do
#     traces+=("$f")
# done

# generate_trace_and_run() {
#     local spectrace=$1
#     local spectrace_name=$(basename "$spectrace" .onnx)

#     echo "Running simulation for $spectrace_name"

#     $CONVENTIONAL_NORMAL_PROGRAM --warmup_instructions 80000000 --simulation_instructions 200000000 --uncompressed_trace -traces "$spectrace_name.champsimtrace.xz" > "../$NORMAL_OUTPUT_DIR/conventional_output/${model_name}.out" 2>&1
#     $REUSE_NORMAL_PROGRAM --warmup_instructions 80000000 --simulation_instructions 200000000 --uncompressed_trace --reuse_cache_llc -traces "$model_name.champsimtrace.xz" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output/${model_name}.out" 2>&1

#     # $CONVENTIONAL_NORMAL_PROGRAM --warmup_instructions 30000000 --simulation_instructions 400000000 --uncompressed_trace -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/conventional_output_40/${model_name}.out" 2>&1
#     # $REUSE_NORMAL_PROGRAM --warmup_instructions 30000000 --simulation_instructions 400000000 --uncompressed_trace --reuse_cache_llc -traces "$model_name.champsim.normal.trace" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output_40/${model_name}.out" 2>&1

#     # $CONVENTIONAL_DATA_PROGRAM --warmup_instructions 50000000 --simulation_instructions 400000000 --uncompressed_trace -traces "$model_name.champsim.data.trace" > "../$NORMAL_OUTPUT_DIR/conventional_output/${model_name}.out" 2>&1
#     # $REUSE_DATA_PROGRAM --warmup_instructions 50000000 --simulation_instructions 400000000 --uncompressed_trace -traces "$model_name.champsim.data.trace" > "../$NORMAL_OUTPUT_DIR/reuse_cache_output/${model_name}.out" 2>&1

#     #rm "$model_name.champsim.normal.trace"
#     # rm "$model_name.champsim.data.trace"
    
#     echo "Simulation for $spectrace_name completed"
# }

run_simulation() {
    local TRACE="$1"
    local BASENAME=$(basename "$TRACE")

    local SIMULATION=400000000
    local WARMUP=80000000
    
    local CONVENTIONAL_OUTPUT_FILE="$NORMAL_OUTPUT_DIR/conventional_output/${BASENAME%.xz}.out"
    # local REUSE_OUTPUT_FILE="$NORMAL_OUTPUT_DIR/reuse_cache_output/${BASENAME%.xz}.out"

    echo "Running trace $BASENAME"
    # echo "$PROGRAM --warmup_instructions $WARMUP --simulation_instructions $SIMULATION -traces $TRACE > $OUTPUT_FILE"
    $CONVENTIONAL_NORMAL_PROGRAM --warmup_instructions "$WARMUP" --simulation_instructions "$SIMULATION" -traces "$TRACE" > "$CONVENTIONAL_OUTPUT_FILE" 2>&1
    # $REUSE_NORMAL_PROGRAM --warmup_instructions "$WARMUP" --simulation_instructions "$SIMULATION" -traces "$TRACE" > "$REUSE_OUTPUT_FILE" 2>&1
    echo "Simulation for $TRACE completed"
}


#Loop through model
for trace in "${traces[@]}"; do
    run_simulation "$TRACES_DIR/$trace" &
    ((job_count++))

    # Limit parallel jobs
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi

done
