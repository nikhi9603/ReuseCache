import os
import re
import matplotlib.pyplot as plt

def extract_filename_component(path):
    return ".".join((path.split('/')[-1].split('.'))[:-1])

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


model_path = "ONNX-Runtime-Inference/data/models"
conventional_cache_path = "MLOutputs_Normal/conventional_output/"
reuse_cache_path = "MLOutputs_Normal/reuse_cache_output/"

stats = {"ConventionalCache": {}, "ReuseCache" : {}}

all_models = os.listdir(model_path)
models = []

for model in all_models:
    name = extract_filename_component(model)
    models.append(name)

    path = os.path.join(conventional_cache_path, name + ".out")
    extract_data(path, stats["ConventionalCache"], name)

    path = os.path.join(reuse_cache_path, name + ".out")
    extract_data(path, stats["ReuseCache"], name)

total_unused = 0
for model in models:
    for cache in ["ConventionalCache", "ReuseCache"]:
        print(f"Cache: {cache}, model: {model}")
        print(f"IPC: {stats[cache][model]['IPC']:.2f}")
        print(f"HitRate: {stats[cache][model]['HitRate']}")
        if cache == "ConventionalCache":
            print(f"TotalLines: {stats[cache][model]['TotalLines']}")
            print(f"Lines with Zero Usage: {stats[cache][model]['EvictionStats'].get(0, 0)}")
            print(f"% Lines with Zero Usage: {stats[cache][model]['EvictionStats'].get(0, 0) / stats[cache][model]['TotalLines'] * 100:.2f}%")
            print(f"% Lines with Single Usage: {stats[cache][model]['EvictionStats'].get(1, 0) / stats[cache][model]['TotalLines'] * 100:.2f}%")
            total_unused += stats[cache][model]['EvictionStats'].get(0, 0) / stats[cache][model]['TotalLines'] * 100
        else:
            print(f"TotalLines: {stats[cache][model]['TotalLines']}")
            print(f"Lines with Zero Usage: {stats[cache][model]['EvictionStats'].get(0, 0)}")
            print(f"% Lines with Zero Usage: {stats[cache][model]['EvictionStats'].get(0, 0) / stats[cache][model]['TotalLines'] * 100:.2f}%")
        print()

    print("IPC Ratio: ", stats["ReuseCache"][model]['IPC'] / stats["ConventionalCache"][model]['IPC'])
    print("Hit Rate Ratio: ", stats["ReuseCache"][model]['HitRate'] / stats["ConventionalCache"][model]['HitRate'])
    print("===============================================================================================")

print("Average % Unused Lines: ", total_unused / len(models))

ipc_ratios = [stats["ReuseCache"][model]['IPC'] / stats["ConventionalCache"][model]['IPC'] for model in models]
hit_rate_ratios = [stats["ReuseCache"][model]['HitRate'] / stats["ConventionalCache"][model]['HitRate'] for model in models]

plt.figure(figsize=(30, 16))
plt.bar(models, ipc_ratios, color='blue')
plt.title('IPC Ratios')
plt.xlabel('model', fontsize=16)
plt.ylabel('IPC Ratio', fontsize=16)
plt.xticks(rotation=45, fontsize=12)
plt.savefig('ipc_ratios_ml.png')


plt.figure(figsize=(30, 16))
plt.bar(models, hit_rate_ratios, color='green')
plt.title('Hit Rate Ratios')
plt.xlabel('model', fontsize=16)
plt.ylabel('Hit Rate Ratio', fontsize=16)
plt.xticks(rotation=45, fontsize=12)
plt.savefig('hit_rate_ratios_ml.png')