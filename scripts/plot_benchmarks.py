#!/usr/bin/env python3
"""Generate benchmark graphs from CSV output files.

Usage:
    python3 plot_benchmarks.py [--dir RESULTS_DIR] [--output GRAPH_DIR]

Reads results_*.csv files and produces publication-quality graphs.
"""

import os
import sys
import csv
import argparse
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

# ─── Style setup ─────────────────────────────────────────────────────────────

plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams.update({
    'font.family': 'sans-serif',
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.titleweight': 'bold',
    'axes.labelsize': 11,
    'figure.dpi': 150,
    'savefig.dpi': 150,
    'figure.facecolor': 'white',
})

COLORS = {
    'serial': '#2196F3',
    'parallel': '#FF5722',
    'aos': '#E91E63',
    'soa': '#4CAF50',
    'flat': '#9C27B0',
    'threads': ['#2196F3', '#4CAF50', '#FF9800', '#F44336', '#9C27B0', '#00BCD4'],
}


def load_csv(filepath):
    """Load benchmark CSV data into a list of dicts."""
    results = []
    if not os.path.exists(filepath):
        return results
    with open(filepath) as f:
        reader = csv.reader(f)
        header = next(reader)  # skip header
        for row in reader:
            if len(row) < 6:
                continue
            entry = {
                'name': row[0],
                'mean_ms': float(row[1]),
                'stddev_ms': float(row[2]),
                'min_ms': float(row[3]),
                'max_ms': float(row[4]),
                'trials': int(row[5]),
            }
            if len(row) > 6:
                entry['times'] = [float(x) for x in row[6:] if x]
            results.append(entry)
    return results


def find_result(results, name_contains):
    """Find a result by partial name match."""
    for r in results:
        if name_contains in r['name']:
            return r
    return None


def find_results(results, name_contains):
    """Find all results matching partial name."""
    return [r for r in results if name_contains in r['name']]


# ─── Graph 1: Thread Scaling Curve ───────────────────────────────────────────

def plot_thread_scaling(p2_data, output_dir):
    """Thread scaling curve for date and borough queries."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5))

    # Date query scaling
    date_serial = find_result(p2_data, 'Date query SERIAL')
    if not date_serial:
        return

    threads_date = [1, 2, 4, 8, 14]
    times_date = []
    speedups_date = []
    for t in threads_date:
        r = find_result(p2_data, f'Date query {t} threads')
        if r:
            times_date.append(r['mean_ms'])
            speedups_date.append(date_serial['mean_ms'] / r['mean_ms'])
        else:
            times_date.append(None)
            speedups_date.append(None)

    # Filter out None values
    valid = [(t, s) for t, s in zip(threads_date, speedups_date) if s is not None]
    if not valid:
        return
    t_vals, s_vals = zip(*valid)

    ax1.plot(t_vals, s_vals, 'o-', color=COLORS['parallel'], linewidth=2.5,
             markersize=8, label='Actual speedup', zorder=3)
    ax1.plot([1, max(t_vals)], [1, max(t_vals)], '--', color='#999999',
             linewidth=1.5, label='Ideal (linear)', alpha=0.7)
    ax1.set_xlabel('Thread Count')
    ax1.set_ylabel('Speedup (×)')
    ax1.set_title('Date Range Query — Thread Scaling')
    ax1.set_xticks(t_vals)
    ax1.legend(loc='upper left')
    ax1.set_ylim(bottom=0)
    # Annotate peak
    peak_idx = np.argmax(s_vals)
    ax1.annotate(f'{s_vals[peak_idx]:.2f}×',
                 xy=(t_vals[peak_idx], s_vals[peak_idx]),
                 xytext=(10, 10), textcoords='offset points',
                 fontsize=10, fontweight='bold', color=COLORS['parallel'],
                 arrowprops=dict(arrowstyle='->', color=COLORS['parallel']))

    # Borough query scaling
    borough_serial = find_result(p2_data, 'Borough query SERIAL')
    if borough_serial:
        threads_bor = [1, 2, 4, 8, 14]
        speedups_bor = []
        for t in threads_bor:
            r = find_result(p2_data, f'Borough query {t} threads')
            if r:
                speedups_bor.append(borough_serial['mean_ms'] / r['mean_ms'])
            else:
                speedups_bor.append(None)

        valid_b = [(t, s) for t, s in zip(threads_bor, speedups_bor) if s is not None]
        if valid_b:
            tb, sb = zip(*valid_b)
            ax2.plot(tb, sb, 'o-', color='#4CAF50', linewidth=2.5,
                     markersize=8, label='Actual speedup', zorder=3)
            ax2.plot([1, max(tb)], [1, max(tb)], '--', color='#999999',
                     linewidth=1.5, label='Ideal (linear)', alpha=0.7)
            ax2.set_xlabel('Thread Count')
            ax2.set_ylabel('Speedup (×)')
            ax2.set_title('Borough Query — Thread Scaling')
            ax2.set_xticks(tb)
            ax2.legend(loc='upper left')
            ax2.set_ylim(bottom=0)
            peak_idx_b = np.argmax(sb)
            ax2.annotate(f'{sb[peak_idx_b]:.2f}×',
                         xy=(tb[peak_idx_b], sb[peak_idx_b]),
                         xytext=(10, 10), textcoords='offset points',
                         fontsize=10, fontweight='bold', color='#4CAF50',
                         arrowprops=dict(arrowstyle='->', color='#4CAF50'))

    plt.tight_layout()
    path = os.path.join(output_dir, 'thread_scaling.png')
    plt.savefig(path)
    plt.close()
    print(f'  ✓ {path}')


# ─── Graph 2: AoS vs SoA Bar Chart ──────────────────────────────────────────

def plot_aos_vs_soa(p3_data, output_dir):
    """Bar chart comparing AoS and SoA query performance."""
    queries = [
        ('Date range', 'AoS date range', 'SoA date range'),
        ('Borough', 'AoS borough', 'SoA borough'),
        ('Geo bbox', 'AoS geo bounding', 'SoA geo bounding'),
        ('Composite', 'AoS composite', 'SoA composite'),
    ]

    labels = []
    aos_times = []
    soa_times = []
    aos_errs = []
    soa_errs = []

    for label, aos_key, soa_key in queries:
        aos = find_result(p3_data, aos_key)
        soa = find_result(p3_data, soa_key)
        if aos and soa:
            labels.append(label)
            aos_times.append(aos['mean_ms'])
            soa_times.append(soa['mean_ms'])
            aos_errs.append(aos['stddev_ms'])
            soa_errs.append(soa['stddev_ms'])

    if not labels:
        return

    x = np.arange(len(labels))
    width = 0.35

    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, aos_times, width, yerr=aos_errs,
                   label='AoS (Array of Structs)', color=COLORS['aos'],
                   alpha=0.85, capsize=5, edgecolor='white', linewidth=0.5)
    bars2 = ax.bar(x + width/2, soa_times, width, yerr=soa_errs,
                   label='SoA (Struct of Arrays)', color=COLORS['soa'],
                   alpha=0.85, capsize=5, edgecolor='white', linewidth=0.5)

    # Add speedup labels
    for i, (a, s) in enumerate(zip(aos_times, soa_times)):
        speedup = a / s
        ax.annotate(f'{speedup:.1f}×', xy=(x[i], max(a, s)),
                    xytext=(0, 12), textcoords='offset points',
                    ha='center', fontsize=11, fontweight='bold', color='#333333')

    ax.set_ylabel('Query Time (ms)')
    ax.set_title('AoS vs SoA Query Performance')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(loc='upper left')
    ax.set_ylim(bottom=0)

    plt.tight_layout()
    path = os.path.join(output_dir, 'aos_vs_soa.png')
    plt.savefig(path)
    plt.close()
    print(f'  ✓ {path}')


# ─── Graph 3: Parse Pipeline Comparison ─────────────────────────────────────

def plot_parse_scaling(p2_data, output_dir):
    """Parse performance: serial vs parallel threading."""
    parse_serial = find_result(p2_data, 'Parse SERIAL')
    if not parse_serial:
        return

    threads = [1, 2, 4, 8, 14]
    parse_times = [parse_serial['mean_ms'] / 1000]  # seconds
    thread_labels = ['Serial']

    for t in threads:
        r = find_result(p2_data, f'Parse PARALLEL {t} threads')
        if r:
            parse_times.append(r['mean_ms'] / 1000)
            thread_labels.append(f'{t}T')

    fig, ax = plt.subplots(figsize=(10, 5.5))
    colors = [COLORS['serial']] + COLORS['threads'][:len(threads)]
    bars = ax.bar(range(len(parse_times)), parse_times, color=colors[:len(parse_times)],
                  alpha=0.85, edgecolor='white', linewidth=0.5)

    for i, (bar, val) in enumerate(zip(bars, parse_times)):
        speedup = parse_times[0] / val
        label = f'{val:.0f}s'
        if i > 0:
            label += f'\n({speedup:.2f}×)'
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 2,
                label, ha='center', va='bottom', fontsize=10, fontweight='bold')

    ax.set_ylabel('Parse Time (seconds)')
    ax.set_title('CSV Parse Scaling with Thread Count')
    ax.set_xticks(range(len(thread_labels)))
    ax.set_xticklabels(thread_labels)
    ax.set_ylim(bottom=0, top=max(parse_times) * 1.2)

    plt.tight_layout()
    path = os.path.join(output_dir, 'parse_scaling.png')
    plt.savefig(path)
    plt.close()
    print(f'  ✓ {path}')


# ─── Graph 4: SoA + OMP Combined Speedup ────────────────────────────────────

def plot_soa_omp_combined(p3_data, output_dir):
    """Combined SoA + OpenMP cumulative speedup curve."""
    # We need: AoS serial baseline, SoA serial, SoA + 1/2/4/8/14T
    aos_serial = find_result(p3_data, 'AoS date range')
    if not aos_serial:
        return

    soa_serial = find_result(p3_data, 'SoA date SERIAL')
    if not soa_serial:
        return

    categories = ['AoS\nSerial', 'SoA\nSerial']
    times = [aos_serial['mean_ms'], soa_serial['mean_ms']]
    speedups = [1.0, aos_serial['mean_ms'] / soa_serial['mean_ms']]

    threads = [1, 2, 4, 8, 14]
    for t in threads:
        r = find_result(p3_data, f'SoA date {t} threads')
        if r:
            categories.append(f'SoA\n+{t}T')
            times.append(r['mean_ms'])
            speedups.append(aos_serial['mean_ms'] / r['mean_ms'])

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5))

    # Time bars
    colors = [COLORS['aos'], COLORS['soa']] + COLORS['threads'][:len(threads)]
    bars = ax1.bar(range(len(times)), times, color=colors[:len(times)],
                   alpha=0.85, edgecolor='white', linewidth=0.5)
    for bar, val in zip(bars, times):
        ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                 f'{val:.1f}', ha='center', va='bottom', fontsize=9, fontweight='bold')
    ax1.set_ylabel('Query Time (ms)')
    ax1.set_title('Date Range Query — All Configurations')
    ax1.set_xticks(range(len(categories)))
    ax1.set_xticklabels(categories, fontsize=9)
    ax1.set_ylim(bottom=0)

    # Cumulative speedup curve
    ax2.plot(range(len(speedups)), speedups, 'o-', color=COLORS['parallel'],
             linewidth=2.5, markersize=8, zorder=3)
    ax2.fill_between(range(len(speedups)), speedups, alpha=0.15,
                     color=COLORS['parallel'])
    for i, (s, cat) in enumerate(zip(speedups, categories)):
        ax2.annotate(f'{s:.1f}×', xy=(i, s),
                     xytext=(0, 12), textcoords='offset points',
                     ha='center', fontsize=10, fontweight='bold')
    ax2.set_ylabel('Speedup vs AoS Serial (×)')
    ax2.set_title('Cumulative Speedup — SoA + OpenMP')
    ax2.set_xticks(range(len(categories)))
    ax2.set_xticklabels(categories, fontsize=9)
    ax2.set_ylim(bottom=0)

    plt.tight_layout()
    path = os.path.join(output_dir, 'soa_omp_combined.png')
    plt.savefig(path)
    plt.close()
    print(f'  ✓ {path}')


# ─── Graph 5: Memory Footprint Comparison ────────────────────────────────────

def plot_memory_comparison(output_dir, aos_mem=0, soa_mem=0, soa_flat=0, csv_size=0):
    """Stacked bar chart of memory footprint comparison."""
    if aos_mem == 0:
        return

    labels = ['Raw CSV', 'SoA\n(flat+intern)', 'SoA\n(flat)', 'SoA\n(vector<str>)', 'AoS']
    values = [csv_size/1e9, soa_flat*0.97/1e9, soa_flat/1e9, soa_mem/1e9, aos_mem/1e9]

    fig, ax = plt.subplots(figsize=(8, 5.5))
    colors = ['#78909C', '#9C27B0', '#4CAF50', '#FF9800', '#E91E63']
    bars = ax.barh(range(len(labels)), values, color=colors,
                   alpha=0.85, edgecolor='white', linewidth=0.5)

    for bar, val in zip(bars, values):
        ax.text(bar.get_width() + 0.1, bar.get_y() + bar.get_height()/2,
                f'{val:.1f} GB', va='center', fontsize=11, fontweight='bold')

    ax.set_xlabel('Memory (GB)')
    ax.set_title('Memory Footprint Comparison')
    ax.set_yticks(range(len(labels)))
    ax.set_yticklabels(labels)
    ax.set_xlim(0, max(values) * 1.3)

    plt.tight_layout()
    path = os.path.join(output_dir, 'memory_comparison.png')
    plt.savefig(path)
    plt.close()
    print(f'  ✓ {path}')


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='Generate benchmark graphs')
    parser.add_argument('--dir', default='.', help='Directory with results CSV files')
    parser.add_argument('--output', default='report/graphs', help='Output directory for graphs')
    parser.add_argument('--aos-mem', type=float, default=0, help='AoS memory in bytes')
    parser.add_argument('--soa-mem', type=float, default=0, help='SoA memory in bytes')
    parser.add_argument('--soa-flat', type=float, default=0, help='SoA flat memory in bytes')
    parser.add_argument('--csv-size', type=float, default=0, help='CSV file size in bytes')
    args = parser.parse_args()

    os.makedirs(args.output, exist_ok=True)

    print('Loading benchmark data...')
    p1 = load_csv(os.path.join(args.dir, 'results_12g_p1.csv'))
    p2 = load_csv(os.path.join(args.dir, 'results_12g_p2.csv'))
    p3 = load_csv(os.path.join(args.dir, 'results_12g_p3.csv'))

    # Fallback to results_full_* if 12g files don't exist
    if not p1:
        p1 = load_csv(os.path.join(args.dir, 'results_full_p1.csv'))
    if not p2:
        p2 = load_csv(os.path.join(args.dir, 'results_full_p2.csv'))
    if not p3:
        p3 = load_csv(os.path.join(args.dir, 'results_full_p3.csv'))

    print(f'  P1: {len(p1)} experiments, P2: {len(p2)} experiments, P3: {len(p3)} experiments')

    print('\nGenerating graphs...')
    if p2:
        plot_thread_scaling(p2, args.output)
        plot_parse_scaling(p2, args.output)
    if p3:
        plot_aos_vs_soa(p3, args.output)
        plot_soa_omp_combined(p3, args.output)

    if args.aos_mem > 0:
        plot_memory_comparison(args.output, args.aos_mem, args.soa_mem,
                              args.soa_flat, args.csv_size)

    print('\nDone! Graphs saved to:', args.output)


if __name__ == '__main__':
    main()
