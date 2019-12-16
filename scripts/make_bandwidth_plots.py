import os
import re
import json
from collections import defaultdict
import matplotlib.pyplot as plt
import matplotlib as mpl
import brewer2mpl
import numpy as np
from helpers import read_time_value

plot_style = {"marker": "o", "linewidth": 4, "markersize": 12}
scatter_style = {"s": 256}
errorbar_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}
font_size = 22

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
	plt.rc('xtick', labelsize=font_size)
	plt.rc('ytick', labelsize=font_size)
	plt.rc('axes', labelsize=font_size, linewidth=1.5)

def get_sub_dirs(folder):
	return [f for f in os.listdir(folder) if os.path.isdir(os.path.join(folder, f))]

def parse_stats_file(filename):
	with open(filename, "r") as f:
		return json.load(f)

def parse_ooc_stats(results_folder):
	results = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: [])))

	for mem_limit_str in get_sub_dirs(results_folder):
		re_match = re.match("geom([0-9]+)_bvh([0-9]+)", mem_limit_str)
		geom_limit = int(re_match.group(1))
		bvh_limit = int(re_match.group(2))

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
					results[scene][geom_limit][culling].append(stats)

	return results


def plot_bandwidth_usage(ooc_stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(ooc_stats.items()):
		for culling in [True, False]:
			x = np.array([int(res) for res in scene_stats.keys()])
			y = np.array([np.mean([disk_bandwidth(run_stats[-1]["data"]) / 1000000000 for run_stats in culling_stats[culling]])
						for culling_stats in scene_stats.values()])
			indices = np.argsort(x)
			x = x[indices][:4]
			y = y[indices][:4]
			if culling:
				ax.plot(x, y, label=f"{scene} - culling",
					color=colors[i], **plot_style, linestyle="--")
			else:
				ax.plot(x, y, label=scene,
					color=colors[i], **plot_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("Memory Limit")
	ax.set_ylabel("Disk Bandwidth") # Per batching point
	#ax.set_xscale("log", basex=2)
	#ax.set_yscale("linear")
	ax.set_ylim(bottom=0)
	#ax.xaxis.set_major_formatter(mpl.ticker.ScalarFormatter())
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter("%dGB"))
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	#fig.savefig("svdag_memory_usage.pdf", bbox_inches="tight")
	plt.show()


if __name__ == "__main__":
	results_folder = "C:/Users/mathijs/Desktop/Euro Graphics/New Results/mem_limit_in_memory_scheds2"
	svdag_results = parse_ooc_stats(results_folder)

	configure_mpl()
	#plot_svdag_res_vs_memory_usage(svdag_results)
	#plot_svdag_traversal_time(svdag_results)
	plot_bandwidth_usage(svdag_results)