import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np
import os
from helpers import read_stat_file, read_time_value
import itertools
import brewer2mpl  # Colors
import matplotlib as mpl
import matplotlib.patches as mpatches


font_size = 22
mpl.use('pdf')
plt.rc('font', family='serif', serif='Times')
plt.rc('text', usetex=False)
plt.rc('xtick', labelsize=font_size)
plt.rc('ytick', labelsize=font_size)
plt.rc('axes', labelsize=font_size, linewidth=1.5)

bar_style = {}
plot_style = {"linestyle": "--", "marker": "o",
              "linewidth": 4, "markersize": 12}
errorbar_style = {"linestyle": "--", "linewidth": 4, "markersize": 12}

xlabels = ("50%", "60%", "70%", "80%", "90%", "100%")


def memory_limit(stats):
    return stats["config"]["memory_limit_percentage"]


def total_render_time(stats):
    return read_time_value(stats["timings"]["total_render_time"])


def total_render_time_no_tail(stats):
    return np.sum([read_time_value(flush["processing_time"]) for flush in stats["flush_info"] if flush["approximate_rays_in_system"] > 12000000])


def disk_bandwidth(stats):
    return stats["memory"]["bot_level_loaded"]["value"]


def plot_total_render_time(stats):
    N = len(xlabels)
    ind = np.arange(N, 0, -1) - 1  # The x locations for the groups

    num_scenes = len(stats)
    width = 0.145  # Width of the bars
    outer_margin = 0.02
    inner_margin = 0.01
    item_width = num_scenes * (2 * width + inner_margin) + \
        outer_margin * (num_scenes - 1) - width

    plots = []

    # brewer2mpl.get_map args: set name  set type  number of colors
    bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
    colors = bmap.mpl_colors

    fig = plt.figure(figsize=(10, 6))
    ax = fig.gca()
    for i, (scene, scene_stats) in enumerate(stats.items()):
        offset = i * (width + inner_margin + width + outer_margin)

        x = [memory_limit(runs[0]) for runs in scene_stats["noculling"]]
        y = [np.mean([total_render_time(x) for x in runs])
             for runs in scene_stats["noculling"]]
        stddev = [np.std([total_render_time(x) for x in runs])
                  for runs in scene_stats["noculling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        plots.append(ax.bar(ind + offset, y, yerr=stddev, width=width,
            bottom=0, color=colors[i], **bar_style))

        x = [memory_limit(runs[0]) for runs in scene_stats["culling"]]
        y = [np.mean([total_render_time(x) for x in runs])
             for runs in scene_stats["culling"]]
        stddev = [np.std([total_render_time(x) for x in runs])
                  for runs in scene_stats["culling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        ax.bar(ind + offset + inner_margin + width,
               y, yerr=stddev, width=width, bottom=0, color=colors[i], hatch="//", **bar_style)


    wo_culling = mpatches.Patch(edgecolor="black", facecolor="gray")
    w_culling = mpatches.Patch(
        edgecolor="black", facecolor="gray", hatch="//")
    legend = plt.legend([wo_culling, w_culling], ("no culling", "culling"), prop={
                        "size": font_size}, frameon=False, loc=(0.35, 0.7715))
    ax.add_artist(legend)
    ax.legend(plots, tuple(stats.keys()), prop={"size": font_size}, frameon=False)

    ax.set_xticks(ind + item_width/2)
    ax.set_xticklabels(xlabels)
    ax.set_xlim(-width, (N-1) + (item_width - outer_margin) + width)
    ax.set_xlabel("Memory limit (%)")
    ax.set_ylabel("Render time (s)")
    fig.tight_layout()
    fig.savefig("ooc_render_time_with_tail.pdf", bbox_inches="tight")
    #plt.show()


def plot_total_render_time_without_tail(stats):
    N = len(xlabels)
    ind = np.arange(N, 0, -1) - 1  # The x locations for the groups

    num_scenes = len(stats)
    width = 0.145  # Width of the bars
    outer_margin = 0.02
    inner_margin = 0.01
    item_width = num_scenes * (2 * width + inner_margin) + \
        outer_margin * (num_scenes - 1) - width

    plots = []

    # brewer2mpl.get_map args: set name  set type  number of colors
    bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
    colors = bmap.mpl_colors

    fig = plt.figure(figsize=(10, 6))
    ax = fig.gca()
    for i, (scene, scene_stats) in enumerate(stats.items()):
        offset = i * (width + inner_margin + width + outer_margin)

        x = [memory_limit(runs[0]) for runs in scene_stats["noculling"]]
        y = [np.mean([total_render_time_no_tail(x) for x in runs])
             for runs in scene_stats["noculling"]]
        stddev = [np.std([total_render_time_no_tail(x) for x in runs])
                  for runs in scene_stats["noculling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        plots.append(ax.bar(ind + offset, y, yerr=stddev, width=width,
                            bottom=0, color=colors[i], **bar_style))

        x = [memory_limit(runs[0]) for runs in scene_stats["culling"]]
        y = [np.mean([total_render_time_no_tail(x) for x in runs])
             for runs in scene_stats["culling"]]
        stddev = [np.std([total_render_time_no_tail(x) for x in runs])
                  for runs in scene_stats["culling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        ax.bar(ind + offset + inner_margin + width,
               y, yerr=stddev, width=width, bottom=0, color=colors[i], hatch="//", **bar_style)


    wo_culling = mpatches.Patch(edgecolor="black", facecolor="gray")
    w_culling = mpatches.Patch(
        edgecolor="black", facecolor="gray", hatch="//")
    legend = plt.legend([wo_culling, w_culling], ("no culling", "culling"), prop={
                        "size": font_size}, frameon=False, loc=(0.35, 0.7715))
    ax.add_artist(legend)
    ax.legend(plots, tuple(stats.keys()), prop={"size": font_size}, frameon=False)

    ax.set_xticks(ind + item_width/2)
    ax.set_xticklabels(xlabels)
    ax.set_xlim(-width, (N-1) + (item_width - outer_margin) + width)
    ax.set_xlabel("Memory limit (%)")
    ax.set_ylabel("Render time (s)")
    fig.tight_layout()
    fig.savefig("ooc_render_time_no_tail.pdf", bbox_inches="tight")
    #plt.show()


def plot_disk_bandwidth(stats):
    N = len(xlabels)
    ind = np.arange(N, 0, -1) - 1  # The x locations for the groups

    num_scenes = len(stats)
    width = 0.145  # Width of the bars
    outer_margin = 0.02
    inner_margin = 0.01
    item_width = num_scenes * (2 * width + inner_margin) + \
        outer_margin * (num_scenes - 1) - width

    plots = []

    # brewer2mpl.get_map args: set name  set type  number of colors
    bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
    colors = bmap.mpl_colors

    fig = plt.figure(figsize=(10, 6))
    ax = fig.gca()
    for i, (scene, scene_stats) in enumerate(stats.items()):
        offset = i * (width + inner_margin + width + outer_margin)
        
        x = [memory_limit(runs[0]) for runs in scene_stats["noculling"]]
        y = [np.mean([disk_bandwidth(x) / 1000000000 for x in runs])
             for runs in scene_stats["noculling"]]
        stddev = [np.std([disk_bandwidth(x) / 1000000000 for x in runs])
                  for runs in scene_stats["noculling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        plots.append(ax.bar(ind + offset, y, yerr=stddev, width=width,
                            bottom=0, color=colors[i], **bar_style))

        x = [memory_limit(runs[0]) for runs in scene_stats["culling"]]
        y = [np.mean([disk_bandwidth(x) / 1000000000 for x in runs])
             for runs in scene_stats["culling"]]
        stddev = [np.std([disk_bandwidth(x) / 1000000000 for x in runs])
                  for runs in scene_stats["culling"]]
        #ax.errorbar(x, y, yerr=stddev, label=label)
        ax.bar(ind + offset + inner_margin + width,
               y, yerr=stddev, width=width, bottom=0, color=colors[i], hatch="//", **bar_style)

    #fig.subplots_adjust(left=.15, bottom=.16, right=.99, top=.97)
    #ax.set_title("Render time without tail (seconds)")
    ax.set_xticks(ind + item_width/2)
    ax.set_xticklabels(xlabels)

    wo_culling = mpatches.Patch(edgecolor="black", facecolor="gray")
    w_culling = mpatches.Patch(
        edgecolor="black", facecolor="gray", hatch="//")
    legend = plt.legend([wo_culling, w_culling], ("no culling", "culling"), prop={
                        "size": font_size}, frameon=False, loc=(0.35,0.7715))
    ax.add_artist(legend)
    ax.legend(plots, tuple(stats.keys()), prop={
              "size": font_size}, frameon=False)
    ax.autoscale_view()
    ax.set_xlim(-width, (N-1) + (item_width - outer_margin) + width)
    ax.set_xlabel("Memory limit (%)")
    ax.set_ylabel("Disk bandwidth (GB)")
    fig.tight_layout()
    fig.savefig("ooc_bandwidth.pdf", bbox_inches="tight")
    #plt.show()


def group_runs(stats):
    ret = {}
    for scene, scene_stats in stats.items():
        ret[scene] = {}

        scene_stats["culling"].sort(key=lambda x: memory_limit(x))
        grouped_stats = [list(g) for k, g in itertools.groupby(
            scene_stats["culling"], memory_limit)]
        ret[scene]["culling"] = grouped_stats

        scene_stats["noculling"].sort(key=lambda x: memory_limit(x))
        grouped_stats = [list(g) for k, g in itertools.groupby(
            scene_stats["noculling"], memory_limit)]
        ret[scene]["noculling"] = grouped_stats
    return ret


if __name__ == "__main__":
    test_folder = "C:/Users/Mathijs/iCloudDrive/thesis/results_morton_order/ooc_culling128"
    stats = {}
    for scene in ["crown", "landscape", "island"]:
        scene_stats = {
            "culling": [],
            "noculling": []
        }
        for memory_limit_percentage in [50, 60, 70, 80, 90, 100]:
            for run in range(3):
                folder_name = f"{scene}_ooc_culling128_{memory_limit_percentage}_run{run}"
                folder_path = os.path.join(test_folder, folder_name)
                if not os.path.exists(os.path.join(folder_path, "stats.json")):
                    print(f"WARNING: stats file missing ({folder_path}/stats.json)")
                    continue
                test_stats = read_stat_file(folder_path)
                test_stats["config"]["memory_limit_percentage"] = memory_limit_percentage
                scene_stats["culling"].append(test_stats)

            for run in range(3):
                folder_name = f"{scene}_ooc_noculling_{memory_limit_percentage}_run{run}"
                folder_path = os.path.join(test_folder, folder_name)
                if not os.path.exists(os.path.join(folder_path, "stats.json")):
                    print(
                        f"WARNING: stats file missing ({folder_path}/stats.json)")
                    continue
                test_stats = read_stat_file(folder_path)
                test_stats["config"]["memory_limit_percentage"] = memory_limit_percentage
                scene_stats["noculling"].append(test_stats)
        stats[scene] = scene_stats

    stats = group_runs(stats)
    plot_total_render_time(stats)
    plot_total_render_time_without_tail(stats)
    plot_disk_bandwidth(stats)
