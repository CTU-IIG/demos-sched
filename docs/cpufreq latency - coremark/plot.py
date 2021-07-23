#!/usr/bin/env python3

import sys
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import seaborn as sns
import statistics
from typing import NamedTuple, Sequence, Tuple

SHOULD_SHOW = False

FILE = "./csv/dataset.csv"
FREQS_FILE = "./csv/freqs.csv"
AVGS_FILE = "./csv/fixed_averages.csv"
FIGSIZE = (8, 4.5)

WINDOW_LENGTH_S = 1 / 1000
with open(FREQS_FILE) as fd:
    FREQS =  fd.readline().strip().split(",")
with open(AVGS_FILE) as fd:
    FIXED_AVG_ITERS_PER_SEC =  [float(avg) for avg in fd.readline().strip().split(",")]

FIG_TITLE = sys.argv[1]
# which plots to render
PLOTS = sys.argv[2:]
if len(PLOTS) == 0:
    PLOTS = ["freq", "period", "latency"]
    print("Rendering all plots... (freq, period, latency)")
    print("To only render some plots, pass the names as arguments to this script (e.g. `py ./plot.py freq period`)")


class DataPoint(NamedTuple):
    raw_label: str
    label: str
    color: Tuple[float, float, float]
    is_alternating: bool
    expected_iters: float
    expected_period: float
    iters_per_s: Sequence[float]
    periods_s: Sequence[float]
    latencies_s: Sequence[float]

PALETTE = sns.color_palette(desat=0.7)
data = []
with open(FILE, "r") as fd:
    for row in fd:
        vals = row.split(",")
        label = vals[0].replace("_", "\n")
        is_fixed = "fixed" in label
        
        expected_iters = None if is_fixed else (FIXED_AVG_ITERS_PER_SEC[int(label[-2])] + FIXED_AVG_ITERS_PER_SEC[int(label[-1])]) / 2
        expected_period = None if is_fixed else 1 / expected_iters
        iters = [float(v) for v in vals[1:]]
        periods = [1/v for v in iters]
        latencies = None if is_fixed else [(p - expected_period) / (p / WINDOW_LENGTH_S) for p in periods]
        
        data.append(DataPoint(
            label,
            "fixed\n" + FREQS[int(label[-1])]
                if is_fixed else "alternating\n" + FREQS[int(label[-2])] + "\n" + FREQS[int(label[-1])],
            PALETTE[0] if is_fixed else PALETTE[1],
            not is_fixed,
            expected_iters,
            expected_period,
            iters,
            periods,
            latencies,
        ))

data = sorted(data, key=lambda point: statistics.mean(point.iters_per_s))
def get_prop(prop_name: str, lst: Sequence[DataPoint] = data):
    return [getattr(item, prop_name) for item in lst]


if "freq" in PLOTS:
    fig, ax = plt.subplots(tight_layout=True, figsize=FIGSIZE)
    sns.boxplot(data=get_prop("iters_per_s"), palette=get_prop("color"),
            ax=ax, saturation=1)
    #sns.violinplot(data=get_prop("iters_per_s"), palette=get_prop("color"),
    #        ax=ax, scale="width", inner="quartiles", saturation=1)
    #plt.setp(ax.collections, alpha=0.5)

    for i, avg in enumerate(get_prop("expected_iters")):
        if avg is None: continue
        ax.hlines(y=avg, xmin=i - 0.5, xmax=i + 0.5, color="r")

    ax.grid(alpha=0.3)

    ax.set_title(FIG_TITLE)
    ax.set_xticklabels(get_prop("label"), rotation=45)
    ax.set_ylabel("iterations / second")

    plt.savefig(".\charts\iterations_per_second.png")
    plt.savefig(".\charts\iterations_per_second.svg")
    if SHOULD_SHOW: plt.show()


if "period" in PLOTS:
    fig, ax = plt.subplots(tight_layout=True, figsize=FIGSIZE)
    sns.boxplot(data=get_prop("periods_s"), palette=get_prop("color"),
            ax=ax, saturation=1)
    #sns.violinplot(data=get_prop("periods_s"), palette=get_prop("color"),
    #        ax=ax, scale="width", inner="quartiles", saturation=1)
    #plt.setp(ax.collections, alpha=0.5)

    for i, avg in enumerate(get_prop("expected_period")):
        if avg is None: continue
        ax.hlines(y=avg, xmin=i - 0.5, xmax=i + 0.5, color="r")

    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, pos: "{:.0f} µs".format(1000 * 1000 * x)))
    ax.grid(alpha=0.3)

    ax.set_title(FIG_TITLE)
    ax.set_xticklabels(get_prop("label"), rotation=45)
    ax.set_ylabel("iteration duration")

    plt.savefig(".\charts\iteration_period.png")
    plt.savefig(".\charts\iteration_period.svg")
    if SHOULD_SHOW: plt.show()


if "latency" in PLOTS:
    data_alt = [d for d in data if d.is_alternating]
    fig, ax = plt.subplots(tight_layout=True, figsize=FIGSIZE)
    sns.boxplot(data=get_prop("latencies_s", data_alt), palette=get_prop("color", data_alt),
            ax=ax, saturation=1)
    #sns.violinplot(data=get_prop("latencies_s", data_alt), palette=get_prop("color", data_alt),
    #        ax=ax, scale="width", inner="quartiles", saturation=1)
    #plt.setp(ax.collections, alpha=0.5)

    ax.set_ylim(bottom=0)
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, pos: "{:.0f} µs".format(1000 * 1000 * x)))
    ax.grid(alpha=0.3)

    ax.set_title(FIG_TITLE)
    ax.set_xticklabels(get_prop("label", data_alt), rotation=45)
    ax.set_ylabel("frequency switch latency")

    plt.savefig(".\charts\latency.png")
    plt.savefig(".\charts\latency.svg")
    if SHOULD_SHOW: plt.show()



#ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, pos: str(int(x)) + " ms"))
#ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, pos: "{:.0f} ms".format(x)))
#for i, (label, iters) in enumerate(zip(labels_freq, iters_per_sec)):
#    ax.annotate(label, xy=(i, max(iters) + 200), ha="center", rotation=45)