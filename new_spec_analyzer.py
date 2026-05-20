import os
import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
from collections import defaultdict

# ------------------------------------------------------------------------------
# CONFIGURATION  ── edit these paths before running
# ------------------------------------------------------------------------------
TRACE_PATH              = "speccpu-traces/"
CONVENTIONAL_CACHE_DIR  = "/scratch/nikhitha/OutputsTemp/final_spec_simulation/conventional_cache_outputs_4mb/"
REUSE_CACHE_DIR         = "/scratch/nikhitha/OutputsTemp/final_spec_simulation/reuse_cache_outputs_8mb_4mb/"

EXCLUDED_TRACES = {
    "400.perlbench-41B.champsimtrace.xz",
    "403.gcc-17B.champsimtrace.xz",
    "445.gobmk-2B.champsimtrace.xz",
    "445.gobmk-30B.champsimtrace.xz",
    "447.dealII-3B.champsimtrace.xz",
    "458.sjeng-31B.champsimtrace.xz",
    "464.h264ref-30B.champsimtrace.xz",
    "464.h264ref-57B.champsimtrace.xz",
    "625.x264_s-12B.champsimtrace.xz",
    "625.x264_s-18B.champsimtrace.xz",
    "625.x264_s-20B.champsimtrace.xz",
    "625.x264_s-33B.champsimtrace.xz",
}

OUTPUT_DIR = "/scratch/nikhitha/OutputsTemp/final_spec_simulation/analysis_8_4_with_4"   # directory where plots are saved
# ------------------------------------------------------------------------------

# -------------------------- colour palette ------------------------------------------
C_CONV   = "#E07B39"   # orange  – conventional
C_REUSE  = "#3A7EBF"   # blue    – reuse
C_RED    = "#D94F3D"
C_GREEN  = "#3DB87A"
C_GRAY   = "#7F8C8D"

PLOT_STYLE = {
    "figure.facecolor": "white",
    "axes.facecolor":   "#F8F9FA",
    "axes.grid":        True,
    "grid.color":       "#DDE1E4",
    "grid.linestyle":   "--",
    "grid.linewidth":   0.6,
    "axes.spines.top":  False,
    "axes.spines.right":False,
    "font.family":      "DejaVu Sans",
}
plt.rcParams.update(PLOT_STYLE)

# ------------------------------------------------------------------------------
# PARSING
# ------------------------------------------------------------------------------

def extract_filename_component(path):
    return ".".join(path.split('/')[-1].split('.')[:-1])


def parse_num_uses_block(lines, start_marker, end_marker):
    """Parse LLC_EVICTED or LLC_RESIDENT num_uses blocks."""
    uses = {}
    inside = False
    for line in lines:
        if start_marker in line:
            inside = True
            continue
        if end_marker in line:
            inside = False
            continue
        if inside:
            # LLC_EVICTED num_uses: 0 count: 22930
            # LLC_RESIDENT num_uses: 0 count: 23299
            t = re.search(r'num_uses:\s*(\d+)\s+count:\s*(\d+)', line)
            if t:
                uses[int(t.group(1))] = int(t.group(2))
    return uses


def extract_data_conventional(path):
    """
    Parse a conventional ChampSim output file.
    Returns dict with keys:
      IPC, HitRate, MPKI,
      EvictedUses  {use_count: line_count},
      ResidentUses {use_count: line_count},
      TotalEvicted, TotalResident,
      L2C_HitRate, L2C_MPKI,
      DRAM_RQ_HitRate
    """
    d = dict(IPC=None, HitRate=None, MPKI=None,
             EvictedUses={}, ResidentUses={},
             TotalEvicted=0, TotalResident=0,
             L2C_HitRate=None, L2C_MPKI=None,
             DRAM_RowBufHit=None, DRAM_RowBufMiss=None,
             AvgMissLatency=None)

    with open(path, 'r') as f:
        lines = f.readlines()

    text = "".join(lines)

    # IPC
    for line in lines:
        if "Finished CPU 0 instructions:" in line:
            d['IPC'] = float(line.split()[9])

        # LLC hit rate from "LLC TOTAL" line (conventional uses HIT %)
        if "LLC DEMAND     ACCESS:" in line and "REUSE CACHE" not in text[:text.find(line)]:
            t = re.search(r'HIT %:\s*([\d.]+)', line)
            if t:
                d['HitRate'] = float(t.group(1))
            m2 = re.search(r'MPKI:\s*([\d.inf]+)', line)
            if m2:
                try:
                    d['MPKI'] = float(m2.group(1))
                except ValueError:
                    d['MPKI'] = float('inf')

        # L2C
        if "L2C TOTAL     ACCESS:" in line:
            t = re.search(r'HIT %:\s*([\d.]+)', line)
            if t:
                d['L2C_HitRate'] = float(t.group(1))
            m2 = re.search(r'MPKI:\s*([\d.inf]+)', line)
            if m2:
                try:
                    d['L2C_MPKI'] = float(m2.group(1))
                except ValueError:
                    pass

        # DRAM row buffer
        if "RQ ROW_BUFFER_HIT:" in line:
            t = re.search(r'ROW_BUFFER_HIT:\s*(\d+)\s+ROW_BUFFER_MISS:\s*(\d+)', line)
            if t:
                d['DRAM_RowBufHit']  = int(t.group(1))
                d['DRAM_RowBufMiss'] = int(t.group(2))

        # Average miss latency
        if "AVERAGE MISS LATENCY: (DEMAND MISSES ONLY)" in line and "LLC" in line:
            t = re.search(r'([\d.]+)\s+cycles', line)
            if t:
                d['AvgMissLatency'] = float(t.group(1))

    # New num_uses block format
    evicted = parse_num_uses_block(
        lines,
        "LLC_EVICTED_BLOCKS_NUM_USES_START",
        "LLC_EVICTED_BLOCKS_NUM_USES_END"
    )
    resident = parse_num_uses_block(
        lines,
        "LLC_RESIDENT_BLOCKS_NUM_USES_START",
        "LLC_RESIDENT_BLOCKS_NUM_USES_END"
    )
    if evicted:
        d['EvictedUses'] = evicted
    if resident:
        d['ResidentUses'] = resident



    # Parse totals
    for line in lines:
        if "LLC_EVICTED_TOTAL:" in line:
            d['TotalEvicted'] = int(line.split()[-1])
        if "LLC_RESIDENT_TOTAL:" in line:
            d['TotalResident'] = int(line.split()[-1])

    return d


def extract_data_reuse(path):
    """
    Parse a REUSE CACHE ChampSim output file.
    Extra fields vs conventional:
      TagMissDataMiss, TagHitDataMiss, TagHitDataHit
      (the decoupled tag/data breakdown)
    """
    d = dict(IPC=None, HitRate=None, MPKI=None,
             EvictedUses={}, ResidentUses={},
             TotalEvicted=0, TotalResident=0,
             TagMissDataMiss=None, TagHitDataMiss=None, TagHitDataHit=None,
             TagMissDataMiss_MPKI=None, TagHitDataMiss_MPKI=None, TagHitDataHit_MPKI=None,
             DemandTagMissDataMiss=None, DemandTagHitDataMiss=None, DemandTagHitDataHit=None, 
             DemandTagMissDataMiss_MPKI=None, DemandTagHitDataMiss_MPKI=None, DemandTagHitDataHit_MPKI=None,
             WBTagHitDataHit=None, WBTagMissDataMiss=None, WBTagHitDataMiss=None,
             L2C_HitRate=None, L2C_MPKI=None,
             DRAM_RowBufHit=None, DRAM_RowBufMiss=None,
             AvgMissLatency=None)

    with open(path, 'r') as f:
        lines = f.readlines()

    in_final_stats = False

    for line in lines:
        if "REUSE CACHE LLC FINAL STATS" in line:
            in_final_stats = True
            continue

        if "Finished CPU 0 instructions:" in line:
            d['IPC'] = float(line.split()[9])

        # # Hit rate: "HIT_RATE: 0.744255" style (reuse cache)
        # if line.strip().startswith("HIT_RATE:") and in_final_stats:
        #     try:
        #         d['HitRate'] = float(line.split()[1]) * 100
        #     except (IndexError, ValueError):
        #         pass

        # Also accept "LLC TOTAL ... HIT %:" inside final stats section
        if in_final_stats and "LLC DEMAND     ACCESS:" in line:
            t = re.search(r'HIT %:\s*([\d.]+)', line)
            if t:
                d['HitRate'] = float(t.group(1))
            m2 = re.search(r'MPKI:\s*([\d.inf]+)', line)
            if m2:
                try:
                    d['MPKI'] = float(m2.group(1))
                except ValueError:
                    d['MPKI'] = float('inf')

        if in_final_stats and "LLC TAG_MISS_DATA_MISS:" in line:
            t = re.search(r'LLC TAG_MISS_DATA_MISS:\s*(\d+)\s+MPKI:\s*([\d.]+)', line)
            if t:
                d['TagMissDataMiss']      = int(t.group(1))
                d['TagMissDataMiss_MPKI'] = float(t.group(2))

        if in_final_stats and "LLC TAG_HIT_DATA_MISS:" in line:
            t = re.search(r'LLC TAG_HIT_DATA_MISS:\s*(\d+)\s+MPKI:\s*([\d.]+)', line)
            if t:
                d['TagHitDataMiss']      = int(t.group(1))
                d['TagHitDataMiss_MPKI'] = float(t.group(2))

        if in_final_stats and "LLC TAG_HIT_DATA_HIT:" in line:
            t = re.search(r'LLC TAG_HIT_DATA_HIT:\s*(\d+)\s+MPKI:\s*([\d.]+)', line)
            if t:
                d['TagHitDataHit']      = int(t.group(1))
                d['TagHitDataHit_MPKI'] = float(t.group(2))

        # Average miss latency (in final stats region)
        if "AVERAGE MISS LATENCY: (DEMAND MISSES ONLY)" in line and in_final_stats:
            t = re.search(r'([\d.]+)\s+cycles', line)
            if t:
                d['AvgMissLatency'] = float(t.group(1))

        # L2C
        if "L2C TOTAL     ACCESS:" in line:
            t = re.search(r'HIT %:\s*([\d.]+)', line)
            if t:
                d['L2C_HitRate'] = float(t.group(1))
            m2 = re.search(r'MPKI:\s*([\d.inf]+)', line)
            if m2:
                try:
                    d['L2C_MPKI'] = float(m2.group(1))
                except ValueError:
                    pass

        # DRAM
        if "RQ ROW_BUFFER_HIT:" in line:
            t = re.search(r'ROW_BUFFER_HIT:\s*(\d+)\s+ROW_BUFFER_MISS:\s*(\d+)', line)
            if t:
                d['DRAM_RowBufHit']  = int(t.group(1))
                d['DRAM_RowBufMiss'] = int(t.group(2))

        if in_final_stats and "LLC DEMAND_TAG_MISS_DATA_MISS:" in line:
            t = re.search(
                r'LLC DEMAND_TAG_MISS_DATA_MISS:\s*(\d+)\s+MPKI:\s*([\d.]+)',
                line
            )
            if t:
                d['DemandTagMissDataMiss'] = int(t.group(1))
                d['DemandTagMissDataMiss_MPKI'] = float(t.group(2))

        if in_final_stats and "LLC DEMAND_TAG_HIT_DATA_MISS:" in line:
            t = re.search(
                r'LLC DEMAND_TAG_HIT_DATA_MISS:\s*(\d+)\s+MPKI:\s*([\d.]+)',
                line
            )
            if t:
                d['DemandTagHitDataMiss'] = int(t.group(1))
                d['DemandTagHitDataMiss_MPKI'] = float(t.group(2))

        if in_final_stats and "LLC DEMAND_TAG_HIT_DATA_HIT:" in line:
            t = re.search(
                r'LLC DEMAND_TAG_HIT_DATA_HIT:\s*(\d+)\s+MPKI:\s*([\d.]+)',
                line
            )
            if t:
                d['DemandTagHitDataHit'] = int(t.group(1))
                d['DemandTagHitDataHit_MPKI'] = float(t.group(2))

        if in_final_stats and "WRITEBACK_TAG_HIT_DATA_HIT:" in line:
            t = re.search(
                r'WRITEBACK_TAG_HIT_DATA_HIT:\s*(\d+)\s+'
                r'WRITEBACK_TAG_MISS_DATA_MISS:\s*(\d+)\s+'
                r'WRITEBACK_TAG_HIT_DATA_MISS:\s*(\d+)',
                line
            )

            if t:
                d['WBTagHitDataHit'] = int(t.group(1))
                d['WBTagMissDataMiss'] = int(t.group(2))
                d['WBTagHitDataMiss'] = int(t.group(3))

    # num_uses blocks
    evicted = parse_num_uses_block(
        lines,
        "LLC_EVICTED_BLOCKS_NUM_USES_START",
        "LLC_EVICTED_BLOCKS_NUM_USES_END"
    )
    resident = parse_num_uses_block(
        lines,
        "LLC_RESIDENT_BLOCKS_NUM_USES_START",
        "LLC_RESIDENT_BLOCKS_NUM_USES_END"
    )
    if evicted:
        d['EvictedUses'] = evicted
    if resident:
        d['ResidentUses'] = resident

    for line in lines:
        if "LLC_EVICTED_TOTAL:" in line:
            d['TotalEvicted'] = int(line.split()[-1])
        if "LLC_RESIDENT_TOTAL:" in line:
            d['TotalResident'] = int(line.split()[-1])

    return d

# ------------------------------------------------------------------------------
# HELPERS
# ------------------------------------------------------------------------------

def safe_pct(num, denom):
    if denom and denom > 0:
        return num / denom * 100
    return 0.0


def zero_use_pct(uses_dict, total):
    """Percentage of cache lines evicted/resident that were never used."""
    return safe_pct(uses_dict.get(0, 0), total)


def single_use_pct(uses_dict, total):
    return safe_pct(uses_dict.get(1, 0), total)


def add_bar_labels(ax, bars, fmt="{:.2f}", fontsize=8, rotation=0, color="black"):
    for bar in bars:
        h = bar.get_height()
        if h > 0:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                h + 0.005 * ax.get_ylim()[1],
                fmt.format(h),
                ha='center', va='bottom',
                fontsize=fontsize, rotation=rotation, color=color
            )


def save(fig, name):
    path = os.path.join(OUTPUT_DIR, name)
    fig.savefig(path, dpi=150, bbox_inches='tight')
    print(f"  Saved: {path}")
    plt.close(fig)

# ------------------------------------------------------------------------------
# PLOTTING FUNCTIONS
# ------------------------------------------------------------------------------

def plot_ipc_comparison(traces, conv, reuse):
    conv_ipc  = [conv[t]['IPC']   for t in traces]
    reuse_ipc = [reuse[t]['IPC']  for t in traces]
    ratios    = [r/c if c else 0 for r, c in zip(reuse_ipc, conv_ipc)]
    avg_ratio = np.mean(ratios)

    fig, axes = plt.subplots(1, 2, figsize=(22, 7))
    x = np.arange(len(traces))
    w = 0.38

    # Side-by-side IPC
    ax = axes[0]
    b1 = ax.bar(x - w/2, conv_ipc,  w, label='Conventional', color=C_CONV,  zorder=3)
    b2 = ax.bar(x + w/2, reuse_ipc, w, label='Reuse Cache',   color=C_REUSE, zorder=3)
    ax.set_title('IPC: Conventional vs Reuse Cache', fontsize=14, fontweight='bold')
    ax.set_ylabel('IPC')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)

    # IPC Ratios
    ax2 = axes[1]
    colors = [C_GREEN if r >= 1 else C_RED for r in ratios]
    bars = ax2.bar(traces, ratios, color=colors, zorder=3, edgecolor='white', linewidth=0.5)
    ax2.axhline(1.0,         color='black',  linestyle='-',  linewidth=1.2, label='Baseline (1.0)')
    ax2.axhline(avg_ratio,   color=C_REUSE,  linestyle='--', linewidth=1.5,
                label=f'Average ({avg_ratio:.3f})')
    ax2.set_title('IPC Ratio (Reuse / Conventional)', fontsize=14, fontweight='bold')
    ax2.set_ylabel('IPC Ratio')
    ax2.set_xticks(range(len(traces)))
    ax2.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax2.legend(fontsize=10)
    add_bar_labels(ax2, bars, fmt="{:.3f}", fontsize=7.5)

    fig.suptitle('IPC Analysis', fontsize=16, fontweight='bold', y=1.01)
    fig.tight_layout()
    save(fig, 'ipc_analysis.png')


def plot_hitrate_comparison(traces, conv, reuse):
    conv_hr  = [conv[t]['HitRate']  for t in traces]
    reuse_hr = [reuse[t]['HitRate'] for t in traces]
    ratios   = [r/c if c else 0 for r, c in zip(reuse_hr, conv_hr)]
    avg_ratio = np.mean(ratios)

    fig, axes = plt.subplots(1, 2, figsize=(22, 7))
    x = np.arange(len(traces))
    w = 0.38

    ax = axes[0]
    ax.bar(x - w/2, conv_hr,  w, label='Conventional', color=C_CONV,  zorder=3)
    ax.bar(x + w/2, reuse_hr, w, label='Reuse Cache',   color=C_REUSE, zorder=3)
    ax.set_title('LLC Hit Rate: Conventional vs Reuse Cache', fontsize=14, fontweight='bold')
    ax.set_ylabel('Hit Rate (%)')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)

    ax2 = axes[1]
    colors = [C_GREEN if r >= 1 else C_RED for r in ratios]
    bars = ax2.bar(traces, ratios, color=colors, zorder=3, edgecolor='white', linewidth=0.5)
    ax2.axhline(1.0,         color='black', linestyle='-',  linewidth=1.2)
    ax2.axhline(avg_ratio,   color=C_REUSE, linestyle='--', linewidth=1.5,
                label=f'Average ({avg_ratio:.3f})')
    ax2.set_title('Hit Rate Ratio (Reuse / Conventional)', fontsize=14, fontweight='bold')
    ax2.set_ylabel('Hit Rate Ratio')
    ax2.set_xticks(range(len(traces)))
    ax2.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax2.legend(fontsize=10)
    add_bar_labels(ax2, bars, fmt="{:.3f}", fontsize=7.5)

    fig.suptitle('LLC Hit Rate Analysis', fontsize=16, fontweight='bold', y=1.01)
    fig.tight_layout()
    save(fig, 'hitrate_analysis.png')


def plot_unused_lines(traces, conv, reuse):
    """
    Show unused (0-use) lines for both EVICTED and RESIDENT blocks,
    for both conventional and reuse cache.
    """
    conv_evict_zero  = [zero_use_pct(conv[t]['EvictedUses'],  conv[t]['TotalEvicted'])  for t in traces]
    reuse_evict_zero = [zero_use_pct(reuse[t]['EvictedUses'], reuse[t]['TotalEvicted']) for t in traces]
    conv_res_zero    = [zero_use_pct(conv[t]['ResidentUses'],  conv[t]['TotalResident'])  for t in traces]
    reuse_res_zero   = [zero_use_pct(reuse[t]['ResidentUses'], reuse[t]['TotalResident']) for t in traces]

    fig, axes = plt.subplots(1, 2, figsize=(22, 7))
    x = np.arange(len(traces))
    w = 0.38

    titles = ['Evicted Blocks with Zero Uses (%)', 'Resident Blocks with Zero Uses (%)']
    data_pairs = [(conv_evict_zero, reuse_evict_zero), (conv_res_zero, reuse_res_zero)]
    filenames = ['evicted', 'resident']

    for ax, (conv_d, reuse_d), title in zip(axes, data_pairs, titles):
        avg_c = np.mean([v for v in conv_d  if v is not None])
        avg_r = np.mean([v for v in reuse_d if v is not None])
        b1 = ax.bar(x - w/2, conv_d,  w, label=f'Conventional (avg {avg_c:.1f}%)', color=C_CONV,  zorder=3)
        b2 = ax.bar(x + w/2, reuse_d, w, label=f'Reuse Cache (avg {avg_r:.1f}%)',   color=C_REUSE, zorder=3)
        ax.axhline(avg_c, color=C_CONV,  linestyle='--', linewidth=1.3, alpha=0.7)
        ax.axhline(avg_r, color=C_REUSE, linestyle='--', linewidth=1.3, alpha=0.7)
        ax.set_title(title, fontsize=13, fontweight='bold')
        ax.set_ylabel('Zero-Use Lines (%)')
        ax.set_xticks(x)
        ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
        ax.legend(fontsize=9)

    fig.suptitle('', fontsize=16, fontweight='bold', y=1.01)
    fig.tight_layout()
    save(fig, 'zero_use_lines.png')


def plot_unused_lines_combined(traces, conv, reuse):
    """
    Show total unused (0-use) lines = evicted + resident,
    for both conventional and reuse cache.
    """
    conv_total_zero  = []
    reuse_total_zero = []

    for t in traces:
        # sum zero-use counts from evicted and resident
        conv_evict_zero_count  = conv[t]['EvictedUses'].get(0, 0)
        conv_res_zero_count    = conv[t]['ResidentUses'].get(0, 0)
        reuse_evict_zero_count = reuse[t]['EvictedUses'].get(0, 0)
        reuse_res_zero_count   = reuse[t]['ResidentUses'].get(0, 0)

        conv_total  = conv[t]['TotalEvicted']  + conv[t]['TotalResident']
        reuse_total = reuse[t]['TotalEvicted'] + reuse[t]['TotalResident']

        conv_zero_sum  = conv_evict_zero_count  + conv_res_zero_count
        reuse_zero_sum = reuse_evict_zero_count + reuse_res_zero_count

        conv_total_zero.append( (conv_zero_sum  / conv_total  * 100) if conv_total  > 0 else None)
        reuse_total_zero.append((reuse_zero_sum / reuse_total * 100) if reuse_total > 0 else None)

    fig, ax = plt.subplots(figsize=(14, 6))
    x = np.arange(len(traces))
    w = 0.38

    avg_c = np.mean([v for v in conv_total_zero  if v is not None])
    avg_r = np.mean([v for v in reuse_total_zero if v is not None])

    ax.bar(x - w/2, conv_total_zero,  w, label=f'Conventional (avg {avg_c:.1f}%)', color=C_CONV,  zorder=3)
    ax.bar(x + w/2, reuse_total_zero, w, label=f'Reuse Cache (avg {avg_r:.1f}%)',  color=C_REUSE, zorder=3)
    ax.axhline(avg_c, color=C_CONV,  linestyle='--', linewidth=1.3, alpha=0.7)
    ax.axhline(avg_r, color=C_REUSE, linestyle='--', linewidth=1.3, alpha=0.7)

    ax.set_title('Total Zero-Use Lines (Evicted + Resident) (%)',
                 fontsize=13, fontweight='bold')
    ax.set_ylabel('Zero-Use Lines (%)')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=9)

    fig.tight_layout()
    save(fig, 'zero_use_lines_combined.png')


def plot_tag_data_breakdown(traces, reuse):
    """
    Stacked bar: TAG_MISS_DATA_MISS / TAG_HIT_DATA_MISS / TAG_HIT_DATA_HIT
    This is the key unique metric of the decoupled tag/data design.
    """
    tag_miss_data_miss = []
    tag_hit_data_miss  = []
    tag_hit_data_hit   = []

    for t in traces:
        r = reuse[t]
        total = (r.get('TagMissDataMiss') or 0) + (r.get('TagHitDataMiss') or 0) + (r.get('TagHitDataHit') or 0)
        if total == 0:
            total = 1
        tag_miss_data_miss.append(safe_pct(r.get('TagMissDataMiss') or 0, total))
        tag_hit_data_miss.append( safe_pct(r.get('TagHitDataMiss')  or 0, total))
        tag_hit_data_hit.append(  safe_pct(r.get('TagHitDataHit')   or 0, total))

    x = np.arange(len(traces))
    fig, ax = plt.subplots(figsize=(18, 7))

    b1 = ax.bar(x, tag_hit_data_hit,   label='TAG_HIT  / DATA_HIT',         color=C_GREEN, zorder=3)
    b2 = ax.bar(x, tag_hit_data_miss,  bottom=tag_hit_data_hit,
                label='TAG_HIT  / DATA_MISS',  color=C_CONV,  zorder=3)
    bottom2 = [a+b for a, b in zip(tag_hit_data_hit, tag_hit_data_miss)]
    b3 = ax.bar(x, tag_miss_data_miss, bottom=bottom2,
                label='TAG_MISS / DATA_MISS',                   color=C_RED,   zorder=3)

    ax.set_title('Reuse Cache LLC Access Breakdown\n(Decoupled Tag / Data Array)', fontsize=14, fontweight='bold')
    ax.set_ylabel('% of Total LLC Accesses')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10, loc='upper right')
    ax.set_ylim(0, 110)

    for i, v in enumerate(tag_hit_data_miss):
        if v > 1:
            ax.text(i, tag_hit_data_hit[i] + v/2, f'{v:.1f}%',
                    ha='center', va='center', fontsize=7.5, color='white', fontweight='bold')

    fig.tight_layout()
    save(fig, 'tag_data_breakdown.png')


def plot_demand_tag_breakdown(traces, reuse):
    tag_miss_data_miss = []
    tag_hit_data_miss = []
    tag_hit_data_hit = []

    for t in traces:
        r = reuse[t]
        total = ( (r.get('DemandTagMissDataMiss') or 0) + (r.get('DemandTagHitDataMiss') or 0) + (r.get('DemandTagHitDataHit') or 0))

        if total == 0:
            total = 1

        tag_miss_data_miss.append(safe_pct(r.get('DemandTagMissDataMiss') or 0, total))
        tag_hit_data_miss.append(safe_pct(r.get('DemandTagHitDataMiss') or 0, total))
        tag_hit_data_hit.append(safe_pct(r.get('DemandTagHitDataHit') or 0, total))

    x = np.arange(len(traces))
    fig, ax = plt.subplots(figsize=(18, 7))

    ax.bar( x, tag_hit_data_hit, label='Demand TAG_HIT / DATA_HIT', color=C_GREEN, zorder=3)
    ax.bar( x, tag_hit_data_miss, bottom=tag_hit_data_hit, label='Demand TAG_HIT / DATA_MISS', color=C_CONV, zorder=3)

    bottom2 = [ a + b for a, b in zip(tag_hit_data_hit, tag_hit_data_miss)]
    ax.bar( x, tag_miss_data_miss, bottom=bottom2, label='Demand TAG_MISS / DATA_MISS', color=C_RED, zorder=3 )

    ax.set_title( 'Demand Access Breakdown in Reuse Cache', fontsize=14, fontweight='bold' )
    ax.set_ylabel('% of Demand LLC Accesses')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10, loc='upper right')
    ax.set_ylim(0, 110)

    for i, v in enumerate(tag_hit_data_miss):
        if v > 1:
            ax.text(i, tag_hit_data_hit[i] + v/2, f'{v:.1f}%',
                    ha='center', va='center', fontsize=7.5, color='white', fontweight='bold')


    fig.tight_layout()
    save(fig, 'demand_tag_breakdown.png')


def plot_writeback_breakdown(traces, reuse):

    wb_hit_hit = []
    wb_hit_miss = []
    wb_miss_miss = []

    for t in traces:

        r = reuse[t]

        total = (
            (r.get('WBTagHitDataHit') or 0) +
            (r.get('WBTagHitDataMiss') or 0) +
            (r.get('WBTagMissDataMiss') or 0)
        )

        if total == 0:
            total = 1

        wb_hit_hit.append(
            safe_pct(r.get('WBTagHitDataHit') or 0, total)
        )

        wb_hit_miss.append(
            safe_pct(r.get('WBTagHitDataMiss') or 0, total)
        )

        wb_miss_miss.append(
            safe_pct(r.get('WBTagMissDataMiss') or 0, total)
        )

    x = np.arange(len(traces))

    fig, ax = plt.subplots(figsize=(18, 7))

    ax.bar(
        x,
        wb_hit_hit,
        label='WB TAG_HIT / DATA_HIT',
        color=C_GREEN,
        zorder=3
    )

    ax.bar(
        x,
        wb_hit_miss,
        bottom=wb_hit_hit,
        label='WB TAG_HIT / DATA_MISS',
        color=C_CONV,
        zorder=3
    )

    bottom2 = [
        a + b for a, b in zip(wb_hit_hit, wb_hit_miss)
    ]

    ax.bar(
        x,
        wb_miss_miss,
        bottom=bottom2,
        label='WB TAG_MISS / DATA_MISS',
        color=C_RED,
        zorder=3
    )

    ax.set_title(
        'Writeback Access Breakdown in Reuse Cache',
        fontsize=14,
        fontweight='bold'
    )

    ax.set_ylabel('% of Writeback Accesses')

    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right')

    ax.legend()

    for i, v in enumerate(wb_hit_miss):
        if v > 1:
            ax.text(i, wb_hit_hit[i] + v/2, f'{v:.1f}%',
                    ha='center', va='center', fontsize=7.5, color='white', fontweight='bold')


    fig.tight_layout()

    save(fig, 'writeback_breakdown.png')

def plot_tag_hit_data_miss_analysis(traces, conv, reuse):
    """
    TAG_HIT_DATA_MISS = tag says block is present but data was already evicted.
    This measures the 'overhead' of decoupled design vs total accesses.
    Compare as MPKI or absolute %.
    """
    thdm_mpki = [reuse[t].get('TagHitDataMiss_MPKI') or 0 for t in traces]
    tmdm_mpki = [reuse[t].get('TagMissDataMiss_MPKI') or 0 for t in traces]
    conv_mpki = [conv[t].get('MPKI') or 0 for t in traces]
    reuse_total_mpki = [reuse[t].get('MPKI') or 0 for t in traces]

    fig, axes = plt.subplots(1, 2, figsize=(22, 7))

    # MPKI breakdown (reuse only: TAG_HIT_DATA_MISS + TAG_MISS_DATA_MISS = total miss MPKI)
    ax = axes[0]
    x = np.arange(len(traces))
    w = 0.38
    ax.bar(x - w/2, tmdm_mpki, w, label='Tag Miss + Data Miss MPKI', color=C_RED,   zorder=3)
    ax.bar(x + w/2, thdm_mpki, w, label='Tag Hit + Data Miss MPKI', color=C_CONV, zorder=3)
    ax.set_title('Reuse Cache: Miss Breakdown by MPKI', fontsize=13, fontweight='bold')
    ax.set_ylabel('MPKI')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=9)

    # Total MPKI: conventional vs reuse
    ax2 = axes[1]
    b1 = ax2.bar(x - w/2, conv_mpki,         w, label='Conventional LLC MPKI', color=C_CONV,  zorder=3)
    b2 = ax2.bar(x + w/2, reuse_total_mpki,   w, label='Reuse Cache LLC MPKI',  color=C_REUSE, zorder=3)
    ax2.set_title('LLC MPKI: Conventional vs Reuse Cache', fontsize=13, fontweight='bold')
    ax2.set_ylabel('MPKI')
    ax2.set_xticks(x)
    ax2.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax2.legend(fontsize=9)

    fig.suptitle('Tag/Data Miss Analysis', fontsize=16, fontweight='bold', y=1.01)
    fig.tight_layout()
    save(fig, 'tag_data_miss_mpki.png')

def plot_dram_pressure(traces, conv, reuse):
    """DRAM row buffer hit rate — lower miss pressure = better locality."""
    def rb_hitrate(d):
        h = d.get('DRAM_RowBufHit')  or 0
        ms = d.get('DRAM_RowBufMiss') or 0
        total = h + ms
        return h / total * 100 if total else 0

    conv_rb  = [rb_hitrate(conv[t])  for t in traces]
    reuse_rb = [rb_hitrate(reuse[t]) for t in traces]

    fig, ax = plt.subplots(figsize=(18, 6))
    x = np.arange(len(traces))
    w = 0.38
    ax.bar(x - w/2, conv_rb,  w, label='Conventional', color=C_CONV,  zorder=3)
    ax.bar(x + w/2, reuse_rb, w, label='Reuse Cache',   color=C_REUSE, zorder=3)
    ax.set_title('DRAM Row Buffer Hit Rate (%)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Row Buffer Hit Rate (%)')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)
    fig.tight_layout()
    save(fig, 'dram_row_buffer_hitrate.png')


def plot_miss_latency(traces, conv, reuse):
    conv_lat  = [conv[t].get('AvgMissLatency')  or 0 for t in traces]
    reuse_lat = [reuse[t].get('AvgMissLatency') or 0 for t in traces]

    if all(v == 0 for v in conv_lat + reuse_lat):
        print("  [skip] No miss latency data found.")
        return

    fig, ax = plt.subplots(figsize=(18, 6))
    x = np.arange(len(traces))
    w = 0.38
    ax.bar(x - w/2, conv_lat,  w, label='Conventional', color=C_CONV,  zorder=3)
    ax.bar(x + w/2, reuse_lat, w, label='Reuse Cache',   color=C_REUSE, zorder=3)
    ax.set_title('Average LLC Demand Miss Latency (cycles)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Cycles')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)
    fig.tight_layout()
    save(fig, 'avg_miss_latency.png')


def plot_llc_mpki(traces, conv, reuse):
    """LLC MPKI: Conventional vs Reuse Cache — single bar chart."""
    conv_mpki  = [conv[t].get('MPKI')  or 0 for t in traces]
    reuse_mpki = [reuse[t].get('MPKI') or 0 for t in traces]
 
    avg_conv  = np.mean([v for v in conv_mpki  if v is not None])
    avg_reuse = np.mean([v for v in reuse_mpki if v is not None])
 
    x = np.arange(len(traces))
    w = 0.38
 
    fig, ax = plt.subplots(figsize=(18, 6))
    b1 = ax.bar(x - w/2, conv_mpki,  w, label=f'Conventional (avg {avg_conv:.2f})',
                color=C_CONV,  zorder=3)
    b2 = ax.bar(x + w/2, reuse_mpki, w, label=f'Reuse Cache  (avg {avg_reuse:.2f})',
                color=C_REUSE, zorder=3)
    ax.axhline(avg_conv,  color=C_CONV,  linestyle='--', linewidth=1.3, alpha=0.75)
    ax.axhline(avg_reuse, color=C_REUSE, linestyle='--', linewidth=1.3, alpha=0.75)
    ax.set_title('LLC MPKI: Conventional vs Reuse Cache', fontsize=14, fontweight='bold')
    ax.set_ylabel('MPKI')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)
    add_bar_labels(ax, b1, fmt="{:.2f}", fontsize=7)
    add_bar_labels(ax, b2, fmt="{:.2f}", fontsize=7)
 
    fig.tight_layout()
    save(fig, 'llc_mpki_comparison.png')

def plot_l2c_mpki(traces, conv, reuse):
    """L2C MPKI: Conventional vs Reuse Cache — single bar chart."""
    conv_mpki  = [conv[t].get('L2C_MPKI')  or 0 for t in traces]
    reuse_mpki = [reuse[t].get('L2C_MPKI') or 0 for t in traces]
 
    avg_conv  = np.mean([v for v in conv_mpki  if v is not None])
    avg_reuse = np.mean([v for v in reuse_mpki if v is not None])
 
    x = np.arange(len(traces))
    w = 0.38
 
    fig, ax = plt.subplots(figsize=(18, 6))
    b1 = ax.bar(x - w/2, conv_mpki,  w, label=f'Conventional (avg {avg_conv:.2f})',
                color=C_CONV,  zorder=3)
    b2 = ax.bar(x + w/2, reuse_mpki, w, label=f'Reuse Cache  (avg {avg_reuse:.2f})',
                color=C_REUSE, zorder=3)
    ax.axhline(avg_conv,  color=C_CONV,  linestyle='--', linewidth=1.3, alpha=0.75)
    ax.axhline(avg_reuse, color=C_REUSE, linestyle='--', linewidth=1.3, alpha=0.75)
    ax.set_title('L2C MPKI: Conventional vs Reuse Cache', fontsize=14, fontweight='bold')
    ax.set_ylabel('MPKI')
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=10)
    add_bar_labels(ax, b1, fmt="{:.2f}", fontsize=7)
    add_bar_labels(ax, b2, fmt="{:.2f}", fontsize=7)
 
    fig.tight_layout()
    save(fig, 'l2c_mpki_comparison.png')

def plot_useful_resident_lines(traces, conv, reuse):

    conv_useful = []
    reuse_useful = []

    for t in traces:

        c = conv[t]
        r = reuse[t]

        conv_total = c['TotalResident']
        reuse_total = r['TotalResident']

        conv_zero = c['ResidentUses'].get(0, 0)
        reuse_zero = r['ResidentUses'].get(0, 0)

        conv_useful.append(
            safe_pct(conv_total - conv_zero, conv_total)
        )

        reuse_useful.append(
            safe_pct(reuse_total - reuse_zero, reuse_total)
        )

    x = np.arange(len(traces))
    w = 0.38

    fig, ax = plt.subplots(figsize=(18, 6))

    ax.bar(
        x - w/2,
        conv_useful,
        w,
        label='Conventional',
        color=C_CONV,
        zorder=3
    )

    ax.bar(
        x + w/2,
        reuse_useful,
        w,
        label='Reuse Cache',
        color=C_REUSE,
        zorder=3
    )

    ax.set_title(
        'Useful Resident LLC Lines (%)',
        fontsize=14,
        fontweight='bold'
    )

    ax.set_ylabel('Useful Resident Lines (%)')

    ax.set_xticks(x)

    ax.set_xticklabels(
        traces,
        rotation=45,
        ha='right'
    )

    ax.legend()
    fig.tight_layout()
    save(fig, 'useful_resident_lines.png')

# ------------------------------------------------------------------------------
# PRINT REPORT
# ------------------------------------------------------------------------------

def print_report(traces, conv, reuse):
    sep = "=" * 110
    print(sep)
    print(f"{'Trace':<35} {'Conv IPC':>9} {'Reuse IPC':>10} {'IPC Ratio':>10} "
          f"{'Conv HR%':>9} {'Reuse HR%':>10} {'HR Ratio':>9} "
          f"{'TagHit/DataMiss%':>17}")
    print(sep)

    for t in traces:
        c, r = conv[t], reuse[t]
        ipc_ratio = r['IPC'] / c['IPC'] if c['IPC'] else float('nan')
        hr_ratio  = r['HitRate'] / c['HitRate'] if c['HitRate'] else float('nan')

        total_rc = (r.get('TagMissDataMiss') or 0) + (r.get('TagHitDataMiss') or 0) + (r.get('TagHitDataHit') or 0)
        thdm_pct = safe_pct(r.get('TagHitDataMiss') or 0, total_rc)

        print(f"{t:<35} {c['IPC']:>9.4f} {r['IPC']:>10.4f} {ipc_ratio:>10.4f} "
              f"{c['HitRate']:>9.2f} {r['HitRate']:>10.2f} {hr_ratio:>9.4f} "
              f"{thdm_pct:>17.2f}%")

    print(sep)

    ipc_ratios = [reuse[t]['IPC'] / conv[t]['IPC'] for t in traces if conv[t]['IPC']]
    hr_ratios  = [reuse[t]['HitRate'] / conv[t]['HitRate'] for t in traces if conv[t]['HitRate']]
    avg_zero_evict_conv  = np.mean([zero_use_pct(conv[t]['EvictedUses'],  conv[t]['TotalEvicted'])  for t in traces])
    avg_zero_evict_reuse = np.mean([zero_use_pct(reuse[t]['EvictedUses'], reuse[t]['TotalEvicted']) for t in traces])

    print(f"\nSummary across {len(traces)} traces:")
    print(f"  Average IPC Ratio      : {np.mean(ipc_ratios):.4f}  (geomean: {np.exp(np.mean(np.log(ipc_ratios))):.4f})")
    print(f"  Average Hit Rate Ratio : {np.mean(hr_ratios):.4f}")
    print(f"  Avg Zero-Use Evicted % — Conventional : {avg_zero_evict_conv:.2f}%")
    print(f"  Avg Zero-Use Evicted % — Reuse Cache  : {avg_zero_evict_reuse:.2f}%")
    print(sep)

def debug_print_extraction(traces, conv, reuse):
    print("\n" + "="*120)
    print("DEBUG: Extracted Values")
    print("="*120)

    for t in traces:
        print(f"\TRACE: {t}")
        print("-"*80)

        c = conv[t]
        r = reuse[t]

        print("CONVENTIONAL:")
        print(f"  IPC: {c.get('IPC')}")
        print(f"  HitRate: {c.get('HitRate')}")
        print(f"  MPKI: {c.get('MPKI')}")
        print(f"  TotalEvicted: {c.get('TotalEvicted')}")
        print(f"  TotalResident: {c.get('TotalResident')}")
        print(f"  Evicted[0]: {c.get('EvictedUses', {}).get(0, 0)}")
        print(f"  Resident[0]: {c.get('ResidentUses', {}).get(0, 0)}")
        print(f"AVERAGE MISS LATENCY: {c.get('AvgMissLatency')}")

        print("\nREUSE CACHE:")
        print(f"  IPC: {r.get('IPC')}")
        print(f"  HitRate: {r.get('HitRate')}")
        print(f"  MPKI: {r.get('MPKI')}")
        print(f"  TotalEvicted: {r.get('TotalEvicted')}")
        print(f"  TotalResident: {r.get('TotalResident')}")
        print(f"  Evicted[0]: {r.get('EvictedUses', {}).get(0, 0)}")
        print(f"  Resident[0]: {r.get('ResidentUses', {}).get(0, 0)}")
        print(f"AVERAGE MISS LATENCY: {r.get('AvgMissLatency')}")


        print("\n  TAG BREAKDOWN (reuse only):")
        print(f"    TagMissDataMiss: {r.get('TagMissDataMiss')}")
        print(f"    TagHitDataMiss: {r.get('TagHitDataMiss')}")
        print(f"    TagHitDataHit:  {r.get('TagHitDataHit')}")

        print("-"*80)

    print("="*120)

# ------------------------------------------------------------------------------
# MAIN
# ------------------------------------------------------------------------------

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    all_traces = [f for f in os.listdir(TRACE_PATH) if f not in EXCLUDED_TRACES]
    traces = []
    conv_stats  = {}
    reuse_stats = {}

    for trace_file in sorted(all_traces):
        name = extract_filename_component(trace_file)

        conv_path  = os.path.join(CONVENTIONAL_CACHE_DIR, name + ".out")
        reuse_path = os.path.join(REUSE_CACHE_DIR,        name + ".out")

        if not os.path.exists(conv_path):
            print(f"  [skip] Missing conventional output: {conv_path}")
            continue
        if not os.path.exists(reuse_path):
            print(f"  [skip] Missing reuse output: {reuse_path}")
            continue

        traces.append(name)
        conv_stats[name]  = extract_data_conventional(conv_path)
        reuse_stats[name] = extract_data_reuse(reuse_path)

    if not traces:
        print("No traces found. Check TRACE_PATH, CONVENTIONAL_CACHE_DIR, REUSE_CACHE_DIR.")
        return

    print(f"\nLoaded {len(traces)} traces: {traces}\n")

    print_report(traces, conv_stats, reuse_stats)

    # ------------ Plots -----------
    print("\nGenerating plots...")

    plot_ipc_comparison(traces, conv_stats, reuse_stats)
    plot_hitrate_comparison(traces, conv_stats, reuse_stats)
    plot_unused_lines(traces, conv_stats, reuse_stats)
    plot_tag_data_breakdown(traces, reuse_stats)
    plot_tag_hit_data_miss_analysis(traces, conv_stats, reuse_stats)
    plot_dram_pressure(traces, conv_stats, reuse_stats)
    plot_miss_latency(traces, conv_stats, reuse_stats)
    plot_unused_lines_combined(traces, conv_stats, reuse_stats)
    plot_useful_resident_lines(traces, conv_stats, reuse_stats)
    plot_llc_mpki(traces, conv_stats, reuse_stats)
    plot_l2c_mpki(traces, conv_stats, reuse_stats)
    plot_demand_tag_breakdown(traces, reuse_stats)
    plot_writeback_breakdown(traces, reuse_stats)

    print("\nDone. All plots saved.")
    # print_instruction_table(traces, conv_stats)
    # debug_print_extraction(traces, conv_stats, reuse_stats)


if __name__ == "__main__":
    main()