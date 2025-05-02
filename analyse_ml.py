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
unused_lines = [stats["ConventionalCache"][model]['EvictionStats'].get(0, 0) / stats["ConventionalCache"][model]['TotalLines'] * 100 for model in models]

plt.figure(figsize=(30, 16))
plt.bar(models, ipc_ratios, color='blue')
plt.title('IPC Ratios', fontsize=20)
plt.xlabel('Model', fontsize=20)
plt.ylabel('IPC Ratio', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('ipc_ratios_ml.png')


plt.figure(figsize=(30, 16))
plt.bar(models, hit_rate_ratios, color='green')
plt.title('Hit Rate Ratios', fontsize=20)
plt.xlabel('Model', fontsize=20)
plt.ylabel('Hit Rate Ratio', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('hit_rate_ratios_ml.png')

plt.figure(figsize=(30, 16))
plt.bar(models, unused_lines, color='red')
plt.axhline(y=total_unused / len(models), color='r', linestyle='--', label='Average % Unused Lines')
plt.text(0, total_unused / len(models) + 0.5, f'Average: {total_unused / len(models):.2f}%', color='r', fontsize=20)
plt.title('Unused Lines %', fontsize=20)
plt.xlabel('Model', fontsize=20)
plt.ylabel('Unused Lines %', fontsize=20)
plt.xticks(rotation=45, fontsize=20)
plt.tight_layout()
plt.savefig('unused_lines_ml.png')

# plot a bar plot with conventianal ipc and reuse ipc sied by side in a bar graph

plt.figure(figsize=(30, 16))
x = range(len(models))
width = 0.35
plt.bar(x, [stats["ReuseCache"][model]['IPC'] for model in models], width, label='Reuse IPC', color='blue')
plt.bar([i + width for i in x], [stats["ConventionalCache"][model]['IPC'] for model in models], width, label='Conventional IPC', color='orange')
plt.title('IPC for Conventional and Reuse Cache', fontsize=20)
plt.xlabel('Model', fontsize=20)
plt.ylabel('IPC', fontsize=20)
plt.xticks([i + width / 2 for i in x], models, rotation=45, fontsize=20)
plt.legend()
plt.tight_layout()
plt.savefig('ipc_conv_reuse_ml.png')

# plot a bar plot with conventianal hit rate and reuse hit rate sied by side in a bar graph
plt.figure(figsize=(30, 16))
x = range(len(models))
width = 0.35
plt.bar(x, [stats["ReuseCache"][model]['HitRate'] for model in models], width, label='Reuse Hit Rate', color='blue')
plt.bar([i + width for i in x], [stats["ConventionalCache"][model]['HitRate'] for model in models], width, label='Conventional Hit Rate', color='orange')
plt.title('Hit Rate for Conventional and Reuse Cache', fontsize=20)
plt.xlabel('Model', fontsize=20)
plt.ylabel('Hit Rate', fontsize=20)
plt.xticks([i + width / 2 for i in x], models, rotation=45, fontsize=20)
plt.legend()
plt.tight_layout()
plt.savefig('hit_rate_conv_reuse_ml.png')
