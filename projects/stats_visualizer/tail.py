import matplotlib.pyplot as plt
from matplotlib import rc
import matplotlib as mpl
import brewer2mpl  # Colors
import numpy as np
import os
from helpers import read_stat_file, read_time_value
import itertools
import argparse

mpl.use('pdf')
plt.rc('font', family='serif', serif='Times')
plt.rc('text', usetex=False)
plt.rc('xtick', labelsize=14)
plt.rc('ytick', labelsize=14)
plt.rc('axes', labelsize=14, linewidth=1.5)

plot_style = {"linestyle": "--", "marker": "o", "markersize": 8}
errorbar_style = {"linestyle": "--", "linewidth": 2, "markersize": 8}


def memory_limit(stats):
    return stats["config"]["memory_limit_percentage"]

def total_render_time(stats):
    return read_time_value(stats["timings"]["total_render_time"])

def disk_bandwidth(stats):
    return stats["memory"]["bot_level_loaded"]["value"]

def processing_time(flush_info):
    return read_time_value(flush_info["processing_time"])

def rays_in_system(flush_info):
    return flush_info["approximate_rays_in_system"]

def rays_flushed(flush_info):
    return np.sum(flush_info["approximate_rays_per_flushed_batching_point"])


def plot_tail(stats):
    fig, (ax1, ax2) = plt.subplots(2, sharex=True)

    # brewer2mpl.get_map args: set name  set type  number of colors
    bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
    colors = bmap.mpl_colors

    for i, (scene, scene_stats) in enumerate(stats.items()):
        run_stats = scene_stats[50][0]# Run 0 at memory limit 50%

        # https://matplotlib.org/gallery/lines_bars_and_markers/line_styles_reference.html
        x = np.cumsum([processing_time(flush) for flush in run_stats["flush_info"]])
        y = [processing_time(flush) for flush in run_stats["flush_info"]]
        ax1.plot(x, y, color=colors[i])#, label=f"{scene}")

        x = np.cumsum([processing_time(flush) for flush in run_stats["flush_info"]])
        y = [rays_in_system(flush) for flush in run_stats["flush_info"]]
        ax2.plot(x, y, label=f"{scene}", color=colors[i])


    ax2.set_xlabel("Time into render (s)")
    #plt.ylabel("Processing time (seconds)")
    #ax1.ylim(bottom=0)
    #ax2.ylim(bottom=0)
    #ax.set_xlim(ax.get_xlim()[::-1])
    ax1.set_ylabel("Processing time (s)")
    ax2.set_ylabel("Rays in system")
    fig.legend(loc=(0.78, 0.82), frameon=False)
    fig.tight_layout()
    #plt.show()
    fig.savefig("tail.pdf", bbox_inches="tight")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
	parser.add_argument("--folder", type=str, help="Folder containing statistics")
    args = parser.parse_args()

    #test_folder = "C:/Users/Mathijs/iCloudDrive/thesis/results_morton_order/ooc_culling128"
    stats = {}
    #for scene in ["crown", "landscape"]:
    #for scene in ["island"]:
    for scene in ["crown", "landscape", "island"]:
        scene_stats = {}
        for memory_limit_percentage in [50, 60, 70, 80, 90]:
            scene_stats[memory_limit_percentage] = []
            for run in range(1):
                folder_name = f"{scene}_ooc_culling128_{memory_limit_percentage}_run{run}"
                test_stats = read_stat_file(os.path.join(args.folder, folder_name))
                test_stats["config"]["memory_limit_percentage"] = memory_limit_percentage
                scene_stats[memory_limit_percentage].append(test_stats)
        stats[scene] = scene_stats
        
    #stats = group_runs(stats)
    plot_tail(stats)
