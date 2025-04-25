import os
import re

def extract_filename_component(path):
    match = re.search(r'(\d+\.\w+-\w+)', path)
    if match:
        return match.group(1)
    return None

def extract_data(path, stats, name):
    stats[name] = {'IPC' : None, 'HitRate': None, 'EvictionStats' : {}, 'TotalLines' : None}

    with open(path, 'r') as file:
        lines = file.readlines()
        for line in lines:
            if "Finished CPU 0 instructions: " in line:
                ipc = float(line.split()[9])
                stats[name]['IPC'] = ipc
            elif "LLC TOTAL     ACCESS:" in line:
                hit_rate = float(line.split()[10])
                stats[name]['HitRate'] = hit_rate
            elif "HIT_RATE:" in line:
                hit_rate = float(line.split()[1])

                stats[name]['HitRate'] = hit_rate * 100
            elif "lines used" in line:
                parts = line.split()
                lines_count = int(parts[0])
                use_count = int(parts[3])
                stats[name]['EvictionStats'][use_count] = lines_count
            elif "Total lines:" in line:
                parts = line.split()
                total_lines = int(parts[2])
                stats[name]['TotalLines'] = total_lines


trace_path = "traces/"
conventional_cache_path = "ConventionalCache/cache_output/"
reuse_cache_path = "ReuseCache/reuse_cache_output/"

stats = {"ConventionalCache": {}, "ReuseCache" : {}}

all_traces = os.listdir(trace_path)
traces = []

for trace in all_traces:
    name = extract_filename_component(trace)
    traces.append(name)

    path = os.path.join(conventional_cache_path, name + ".champsimtrace.out")
    extract_data(path, stats["ConventionalCache"], name)

    path = os.path.join(reuse_cache_path, name + ".champsimtrace.out")
    extract_data(path, stats["ReuseCache"], name)

for trace in traces:
    if trace == "464.h264ref-64B":
        continue
    for cache in ["ConventionalCache", "ReuseCache"]:
        print(f"Cache: {cache}, Trace: {trace}")
        print(f"IPC: {stats[cache][trace]['IPC']:.2f}")
        print(f"HitRate: {stats[cache][trace]['HitRate']}")
        if cache == "ConventionalCache":
            print(f"TotalLines: {stats[cache][trace]['TotalLines']}")
            print(f"Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0)}")
            print(f"% Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0) / stats[cache][trace]['TotalLines'] * 100:.2f}%")
            print(f"% Lines with Single Usage: {stats[cache][trace]['EvictionStats'].get(1, 0) / stats[cache][trace]['TotalLines'] * 100:.2f}%")
        print()
    print("===============================================================================================")