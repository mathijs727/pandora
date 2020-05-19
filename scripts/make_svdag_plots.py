import os
import json
from collections import defaultdict
import matplotlib.pyplot as plt
import matplotlib as mpl
import matplotlib.patches as mpatches
import brewer2mpl
import numpy as np
from helpers import read_time_value

axis_font_size = 24
axis_tick_font_size = 22
legend_font_size = 24

plot_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}
scatter_style = {"s": 256}
errorbar_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}

def configure_mpl():
	mpl.use("TkAgg")
	plt.rc('font', family='serif', serif='Times')
	plt.rc('text', usetex=False)
	plt.rc('xtick', labelsize=axis_tick_font_size)
	plt.rc('ytick', labelsize=axis_tick_font_size)
	plt.rc('axes', labelsize=axis_font_size, linewidth=1.5)

	# https://tex.stackexchange.com/questions/77968/how-do-i-avoid-type3-fonts-when-submitting-to-manuscriptcentral
	mpl.rcParams['pdf.fonttype'] = 42
	mpl.rcParams['ps.fonttype'] = 42

def get_sub_dirs(folder):
	return [f for f in os.listdir(folder) if os.path.isdir(os.path.join(folder, f))]

def parse_stats_file(filename):
	with open(filename, "r") as f:
		return json.load(f)

def parse_svdag_stats(results_folder):
	results = defaultdict(lambda: defaultdict(lambda: []))

	svdag_res_folder = os.path.join(results_folder, "svdag_res")
	for res in get_sub_dirs(svdag_res_folder):
		if res == "1024":
			continue
		res_folder = os.path.join(svdag_res_folder, res)
		for scene in get_sub_dirs(res_folder):
			scene_folder = os.path.join(res_folder, scene)
			for run in get_sub_dirs(scene_folder):
				run_folder = os.path.join(scene_folder, run)
				stats_file = os.path.join(run_folder, "stats.json")
				stats = parse_stats_file(stats_file)
				results[scene][res].append(stats)

	return results

def num_batching_points(stats):
	return  stats["config"]["ooc"]["num_batching_points"]["value"]

def svdag_resolution(stats):
	return stats["config"]["ooc"]["svdag_resolution"]

def svdag_traversal_time(stats):
	return read_time_value(stats["timings"]["svdag_traversal_time"])

def svdag_traversal_time(stats): # In miliseconds
	return read_time_value(stats["timings"]["svdag_traversal_time"])

def svdag_normalized_traversal_time(stats): # In miliseconds
	return read_time_value(stats["timings"]["svdag_traversal_time"]) / num_batching_points(stats)

def total_render_time(stats):
	return read_time_value(stats["timings"]["total_render_time"])

def culling_percentage(stats):
	return stats["svdag"]["num_rays_culled"]["value"] / stats["svdag"]["num_intersection_tests"]["value"] * 100

def svdag_normalized_memory_after_compression(stats):
	return stats["memory"]["svdags_after_compression"]["value"] / num_batching_points(stats)

def svdag_memory_after_compression(stats):
	return stats["memory"]["svdags_after_compression"]["value"]

def svdag_memory_before_compression(stats):
	return stats["memory"]["svdags_before_compression"]["value"]

def plot_svdag_traversal_time(svdag_stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(svdag_stats.items()):
		x = np.array([int(res) for res in scene_stats.keys()])
		y = np.array([np.mean([svdag_traversal_time(run_stats[-1]["data"]) for run_stats in runs])
					for runs in scene_stats.values()])
		indices = np.argsort(x)
		x = x[indices][:4]
		y = y[indices][:4]
		ax.plot(x, y, label=scene,
					color=colors[i], **plot_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Traversal time") # Per batching point
	ax.set_xscale("log", basex=2)
	ax.set_yscale("linear")
	ax.set_ylim(bottom=0)
	ax.xaxis.set_major_formatter(mpl.ticker.ScalarFormatter())
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter("%dms"))
	ax.legend(prop={"size": legend_font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_memory_usage.pdf", bbox_inches="tight")
	plt.show()


def plot_svdag_memory_usage(svdag_stats):
	N = len(list(svdag_stats.values())[0])
	ind = np.arange(0, N)  # The x locations for the groups

	num_scenes = len(svdag_stats)
	width = 0.2  # Width of the bars
	inner_margin = 0.05

	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Paired', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(12, 8))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(svdag_stats.items()):
		x = np.array([int(res) for res in scene_stats.keys()])
		y = np.array([np.mean([svdag_memory_after_compression(run_stats[-1]["data"]) / 1000000 for run_stats in runs])
					for runs in scene_stats.values()])
		indices = np.argsort(x)
		x = x[indices]
		y = y[indices]

		offset = (i - num_scenes//2) * (width + inner_margin)
		xxx=  ind + offset
		ax.bar(xxx, y, width=width, bottom=0, color=colors[i], edgecolor="black", linewidth=1, label=scene.title())

	ax.set_xticks(ind)
	x = [int(res) for res in list(svdag_stats.values())[0].keys()]
	ax.set_xticklabels(sorted(x))

	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Memory Usage") # Per batching point
	#ax.set_yscale("log")
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter("%dMB"))
	ax.set_axisbelow(True)
	ax.grid(linestyle="dotted")
	ax.legend(prop={"size": legend_font_size}, frameon=True)
	fig.tight_layout()
	fig.savefig("svdag_memory_usage.png", bbox_inches="tight")
	#plt.show()


def plot_svdag_cull_percentage(svdag_stats):
	N = len(list(svdag_stats.values())[0])
	ind = np.arange(0, N)  # The x locations for the groups

	num_scenes = len(svdag_stats)
	width = 0.2  # Width of the bars
	inner_margin = 0.05

	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Paired', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(12, 8))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(svdag_stats.items()):
		print(scene)
		x = np.array([int(res) for res in scene_stats.keys()])
		y = np.array([np.mean([culling_percentage(run_stats[-1]["data"]) for run_stats in runs])
					for runs in scene_stats.values()])
		indices = np.argsort(x)
		x = x[indices]
		y = y[indices]
		#ax.plot(x, y, label=scene,
		#			color=colors[i], **plot_style)

		offset = (i - num_scenes//2) * (width + inner_margin)
		bar = ax.bar(ind + offset, y, width=width, bottom=0, color=colors[i], edgecolor="black", linewidth=1)
		bar.set_label(scene.title())

	ax.set_xticks(ind)
	x = [int(res) for res in list(svdag_stats.values())[0].keys()]
	ax.set_xticklabels(sorted(x))

	#ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	#ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Effectiveness over AABBs") # Per batching point
	#ax.set_xscale("log", basex=2)
	ax.set_yscale("linear")
	#ax.xaxis.set_major_formatter(mpl.ticker.ScalarFormatter())
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter("%d%%"))
	#ax.legend(prop={"size": font_size}, frameon=False)
	ax.set_axisbelow(True)
	ax.grid(linestyle="dotted")

	ax.legend(prop={"size": legend_font_size})

	fig.tight_layout()
	fig.savefig("svdag_culling_percentage.png", bbox_inches="tight")
	plt.show()

if __name__ == "__main__":
	#results_folder = "C:/Users/Mathijs/OneDrive/TU Delft/Projects/Batched Ray Traversal/Results/"
	#results_folder = "C:/Users/Mathijs/Desktop/results_from_submission/"
	results_folder = "D:/Backups/EG2020-final/submission/results/"
	svdag_results = parse_svdag_stats(results_folder)

	configure_mpl()
	#plot_svdag_traversal_time(svdag_results)
	#plot_svdag_memory_usage(svdag_results)
	plot_svdag_cull_percentage(svdag_results)