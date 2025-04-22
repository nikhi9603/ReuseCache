#!/bin/bash

# Download the SPEC CPU2017 traces in parallel

BASE_URL="https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/"
TRACES=(
    400.perlbench-41B.champsimtrace.xz
    400.perlbench-50B.champsimtrace.xz
    401.bzip2-7B.champsimtrace.xz
    403.gcc-16B.champsimtrace.xz
    403.gcc-17B.champsimtrace.xz
    403.gcc-48B.champsimtrace.xz
    433.milc-127B.champsimtrace.xz
    444.namd-44B.champsimtrace.xz
    445.gobmk-17B.champsimtrace.xz
    445.gobmk-2B.champsimtrace.xz
    464.h264ref-30B.champsimtrace.xz
    464.h264ref-57B.champsimtrace.xz
    464.h264ref-64B.champsimtrace.xz
    444.namd-23B.champsimtrace.xz
    445.gobmk-30B.champsimtrace.xz
    445.gobmk-36B.champsimtrace.xz
    447.dealII-3B.champsimtrace.xz
    458.sjeng-31B.champsimtrace.xz
    625.x264_s-12B.champsimtrace.xz
    625.x264_s-18B.champsimtrace.xz
    625.x264_s-20B.champsimtrace.xz
    625.x264_s-33B.champsimtrace.xz
    625.x264_s-39B.champsimtrace.xz
    657.xz_s-56B.champsimtrace.xz
)

mkdir -p speccpu-traces
cd speccpu-traces || exit 1

# Number of parallel jobs (you can also hardcode this)
NUM_JOBS=$(nproc)

# Counter to track background jobs
job_count=0

download_trace() {
    local trace=$1
    echo "Downloading: $trace"
    curl -O "$BASE_URL/$trace" || echo "❌ Failed to download $trace"
}

for trace in "${TRACES[@]}"; do
    download_trace "$trace" &

    ((job_count++))

    # Limit parallel jobs
    if (( job_count >= NUM_JOBS )); then
        wait
        job_count=0
    fi
done

# Final wait for remaining background jobs
wait

echo "✅ All downloads completed."
