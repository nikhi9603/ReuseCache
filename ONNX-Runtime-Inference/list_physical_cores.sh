#!/bin/bash

echo "Logical CPUs (one per physical core):"
for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
    cpu_id=$(basename "$cpu" | sed 's/cpu//')
    siblings=$(cat "$cpu/topology/thread_siblings_list")
    # Only print the *lowest* numbered CPU per physical core
    first_sibling=$(echo $siblings | cut -d',' -f1)
    if [[ "$cpu_id" == "$first_sibling" ]]; then
        echo "$cpu_id"
    fi
done

