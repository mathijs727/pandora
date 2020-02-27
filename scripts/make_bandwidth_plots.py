import os
import re
import json
from collections import defaultdict
import matplotlib.pyplot as plt
import matplotlib as mpl
import matplotlib.patches as mpatches
import brewer2mpl
import numpy as np
from helpers import read_time_value

axis_font_size = 16
axis_tick_font_size = 14
legend_font_size = 15

plot_style = {"marker": "o", "linewidth": 4, "markersize": 12}
scatter_style = {"s": 256}
errorbar_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}

def memory_limit(stats):
	return stats["config"]["memory_limit_percentage"]

def total_render_time(stats):
	return read_time_value(stats["timings"]["total_render_time"])

def total_render_time_no_tail(stats):
	return np.sum([read_time_value(flush["processing_time"]) for flush in stats["flush_info"] if flush["approximate_rays_in_system"] > 12000000])

def disk_bandwidth(stats):
	return stats["memory"]["geometry_loaded"]["value"]

def configure_mpl():
	#mpl.use('pdf')
	mpl.use("TkAgg")
	plt.rc('font', family='serif', serif='Times')
	plt.rc('text', usetex=False)
	plt.rc('xtick', labelsize=axis_tick_font_size)
	plt.rc('ytick', labelsize=axis_tick_font_size)
	plt.rc('axes', labelsize=axis_font_size, linewidth=1, labelpad=10)

def get_sub_dirs(folder):
	return [f for f in os.listdir(folder) if os.path.isdir(os.path.join(folder, f))]

def parse_stats_file(filename):
	with open(filename, "r") as f:
		return json.load(f)

def parse_ooc_stats(results_folder):
	results = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: [])))

	for mem_limit_str in get_sub_dirs(results_folder):
		#re_match = re.match("geom([0-9]+)_bvh([0-9]+)", mem_limit_str)
		#geom_limit = int(re_match.group(1))
		#bvh_limit = int(re_match.group(2))
		mem_limit = int(mem_limit_str)

		mem_lim_folder = os.path.join(results_folder, mem_limit_str)
		for culling_str, culling in [("culling", True), ("no-culling", False)]:
			culling_folder = os.path.join(mem_lim_folder, culling_str)
			for scene in get_sub_dirs(culling_folder):
				scene_folder = os.path.join(culling_folder, scene)
				for run in get_sub_dirs(scene_folder):
					run_folder = os.path.join(scene_folder, run)
					stats_file = os.path.join(run_folder, "stats.json")
					if not os.path.exists(stats_file):
						print("WARNING: stats file {stats_file} does not exist")
						continue
					stats = parse_stats_file(stats_file)
					results[scene][mem_limit][culling].append(stats)

	return results


def plot_total_render_time(ooc_stats):
	xlabels = ("60%", "70%", "80%", "90%")#, "100%")

	N = len(xlabels)
	ind = np.arange(0, N)  # The x locations for the groups

	width = 0.2  # Width of the bars
	inner_margin = 0.05

	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(6, 6))
	ax = fig.gca()
	bars = []
	for i, (_, scene_stats) in enumerate(ooc_stats.items()):
		offset = (i - len(ooc_stats)//2) * (width + inner_margin)

		for culling in [False, True]:
			x = np.array([int(lim) for lim in scene_stats.keys()])
			y = np.array([np.mean([total_render_time(run_stats[-1]["data"]) for run_stats in culling_stats[culling]])
						for culling_stats in scene_stats.values()])
			indices = np.argsort(x)[:-1]
			x = x[indices]
			y = y[indices]
			if culling:
				ax.bar(ind + offset, y, width=width, bottom=0, color=colors[i], hatch="//", edgecolor="black", linewidth=1)
			else:
				bar = ax.bar(ind + offset, y, width=width, bottom=0, color=colors[i], edgecolor="black", linewidth=1)
				bars.append(bar)

	ax.set_xticks(ind)
	ax.set_xticklabels(xlabels)
	
	ax.set_xlabel("Geometry Memory Limit")
	ax.set_ylabel("Render Time") # Per batching point
	ax.set_ylim(bottom=0)
	#ax.yaxis.set_major_formatter(mpl.ticker.StrMethodFormatter("{x:.0e}s"))
	ax.yaxis.set_major_formatter(mpl.ticker.StrMethodFormatter("{x:.0f}s"))

	fig.tight_layout()

	# Shrink current axis's height by 20% on the bottom.
	box = ax.get_position()
	ax.set_position([box.x0, box.y0 + 0.2 * box.height,
					 box.width, box.height * 0.8], which="both")

	# Put a legend below current axis.
	wo_culling = mpatches.Patch(edgecolor="black", facecolor="gray")
	w_culling = mpatches.Patch(edgecolor="black", facecolor="gray", hatch="//")
	ax.legend(
		bars + [wo_culling, w_culling],
		[scene.title() for scene in ooc_stats.keys()] + ["Reference", "Proxy Geometry"],
		prop={"size": legend_font_size},
		ncol=2,
		loc="upper center",
		bbox_to_anchor=(0.5, -0.19),
		frameon=False,
		fancybox=False,
		shadow=False)

	#fig.savefig("total_render_time.pdf", bbox_inches="tight")
	plt.show()


def plot_bandwidth_usage(ooc_stats):
	xlabels = ("60%", "70%", "80%", "90%")#, "100%")

	N = len(xlabels)
	ind = np.arange(0, N)  # The x locations for the groups

	width = 0.2  # Width of the bars
	inner_margin = 0.05

	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(12, 6))
	ax = fig.gca()
	bars = []
	for i, (_, scene_stats) in enumerate(ooc_stats.items()):
		offset = (i - len(ooc_stats)//2) * (width + inner_margin)

		for culling in [False, True]:
			x = np.array([int(lim) for lim in scene_stats.keys()])
			y = np.array([np.mean([disk_bandwidth(run_stats[-1]["data"]) / 1000000000 for run_stats in culling_stats[culling]])
						for culling_stats in scene_stats.values()])
			indices = np.argsort(x)[:-1]
			x = x[indices]
			y = y[indices]
			if culling:
				ax.bar(ind + offset, y, width=width, bottom=0, color=colors[i], hatch="//", edgecolor="black", linewidth=1)
				pass
			else:
				bar = ax.bar(ind + offset, y, width=width, bottom=0, color=colors[i], edgecolor="black", linewidth=1)
				bars.append(bar)

	ax.set_xticks(ind)
	ax.set_xticklabels(xlabels)
	
	ax.set_xlabel("Geometry Memory Limit")
	ax.set_ylabel("Disk Bandwidth") # Per batching point
	ax.set_ylim(bottom=0)
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter("%dGB"))

	wo_culling = mpatches.Patch(edgecolor="black", facecolor="gray")
	w_culling = mpatches.Patch(edgecolor="black", facecolor="gray", hatch="//")
	ax.legend(
		bars + [wo_culling, w_culling],
		[scene.title() for scene in ooc_stats.keys()] + ["Standard", "Early-Out"],
		prop={"size": legend_font_size},
		ncol=2)

	fig.tight_layout()
	fig.savefig("bandwidth_usage.pdf", bbox_inches="tight")
	#plt.show()


if __name__ == "__main__":
	results_folder = "C:/Users/mathi/Desktop/Results/mem_limit/"
	ooc_results = parse_ooc_stats(results_folder)

	configure_mpl()
	#plot_bandwidth_usage(svdag_results)
	plot_total_render_time(ooc_results)
