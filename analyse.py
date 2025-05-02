import os
import re
import matplotlib.pyplot as plt

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

total_unused = 0

for trace in traces:
    for cache in ["ConventionalCache", "ReuseCache"]:
        print(f"Cache: {cache}, Trace: {trace}")
        print(f"IPC: {stats[cache][trace]['IPC']:.2f}")
        print(f"HitRate: {stats[cache][trace]['HitRate']}")
        if cache == "ConventionalCache":
            print(f"TotalLines: {stats[cache][trace]['TotalLines']}")
            print(f"Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0)}")
            print(f"% Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0) / stats[cache][trace]['TotalLines'] * 100:.2f}%")
            print(f"% Lines with Single Usage: {stats[cache][trace]['EvictionStats'].get(1, 0) / stats[cache][trace]['TotalLines'] * 100:.2f}%")
            total_unused += stats[cache][trace]['EvictionStats'].get(0, 0) / stats[cache][trace]['TotalLines'] * 100
        else:
            print(f"TotalLines: {stats[cache][trace]['TotalLines']}")
            print(f"Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0)}")
            print(f"% Lines with Zero Usage: {stats[cache][trace]['EvictionStats'].get(0, 0) / stats[cache][trace]['TotalLines'] * 100:.2f}%")
        print()

    print("IPC Ratio: ", stats["ReuseCache"][trace]['IPC'] / stats["ConventionalCache"][trace]['IPC'])
    print("Hit Rate Ratio: ", stats["ReuseCache"][trace]['HitRate'] / stats["ConventionalCache"][trace]['HitRate'])
    print("===============================================================================================")

print(f"Average % Unused Lines: {total_unused / len(traces):.2f}%")

# Plotting the IPC ratio,  Hit Rate ratio, unused line % in separate plots
ipc_ratios = [stats["ReuseCache"][trace]['IPC'] / stats["ConventionalCache"][trace]['IPC'] for trace in traces]
hit_rate_ratios = [stats["ReuseCache"][trace]['HitRate'] / stats["ConventionalCache"][trace]['HitRate'] for trace in traces]
unused_lines = [stats["ConventionalCache"][trace]['EvictionStats'].get(0, 0) / stats["ConventionalCache"][trace]['TotalLines'] * 100 for trace in traces]

plt.figure(figsize=(30, 16))
plt.bar(traces, ipc_ratios, color='blue')
plt.title('IPC Ratios', fontsize=20)
plt.xlabel('Trace', fontsize=20)
plt.ylabel('IPC Ratio', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('ipc_ratios_spec.png')


plt.figure(figsize=(30, 16))
plt.bar(traces, hit_rate_ratios, color='green')
plt.title('Hit Rate Ratios', fontsize=20)
plt.xlabel('Trace', fontsize=20)
plt.ylabel('Hit Rate Ratio', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('hit_rate_ratios_spec.png')

# add the average line to the plot and write the average value on the plot
plt.figure(figsize=(30, 16))
plt.bar(traces, unused_lines, color='red')
plt.axhline(y=total_unused / len(traces), color='r', linestyle='--', label='Average % Unused Lines')
plt.text(-1, total_unused / len(traces) + 1, f'Average: {total_unused / len(traces):.2f}%', color='r', fontsize=20)
plt.title('Unused Lines %', fontsize=20)
plt.xlabel('Trace', fontsize=20)
plt.ylabel('Unused Lines %', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('unused_lines_spec.png')

plt.figure(figsize=(30, 16))
x = range(len(traces))
width = 0.35
plt.bar(x, [stats["ReuseCache"][trace]['IPC'] for trace in traces], width, label='Reuse IPC', color='blue')
plt.bar([i + width for i in x], [stats["ConventionalCache"][trace]['IPC'] for trace in traces], width, label='Conventional IPC', color='orange')
plt.title('IPC for Conventional and Reuse Cache', fontsize=20)
plt.xlabel('Trace', fontsize=20)
plt.ylabel('IPC', fontsize=20)
plt.xticks([i + width / 2 for i in x], traces, rotation=45, fontsize=20)
plt.legend()
plt.tight_layout()
plt.savefig('ipc_conv_reuse_spec.png')

# plot a bar plot with conventianal ipc and reuse hit rate sied by side in a bar graph
plt.figure(figsize=(30, 16))
x = range(len(traces))
width = 0.35
plt.bar(x, [stats["ReuseCache"][trace]['HitRate'] for trace in traces], width, label='Reuse Hit Rate', color='blue')
plt.bar([i + width for i in x], [stats["ConventionalCache"][trace]['HitRate'] for trace in traces], width, label='Conventional Hit Rate', color='orange')
plt.title('Hit Rate for Conventional and Reuse Cache', fontsize=20)
plt.xlabel('Trace', fontsize=20)
plt.ylabel('Hit Rate', fontsize=20)
plt.xticks([i + width / 2 for i in x], traces, rotation=45, fontsize=20)
plt.legend()
plt.tight_layout()
plt.savefig('hit_rate_conv_reuse_spec.png')