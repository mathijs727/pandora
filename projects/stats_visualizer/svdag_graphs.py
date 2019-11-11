import matplotlib.pyplot as plt
import numpy as np
import os
from helpers import discover_read_stat_files, read_time_value
import itertools
import brewer2mpl  # Colors
import matplotlib as mpl

font_size = 22
mpl.use('pdf')
plt.rc('font', family='serif', serif='Times')
plt.rc('text', usetex=False)
plt.rc('xtick', labelsize=font_size)
plt.rc('ytick', labelsize=font_size)
plt.rc('axes', labelsize=font_size, linewidth=1.5)

plot_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}
errorbar_style = {"linestyle": "--", "marker": "o", "linewidth": 4, "markersize": 12}

def svdag_resolution(stats):
	return stats["config"]["ooc"]["svdag_resolution"]

def svdag_traversal_time(stats):
	return read_time_value(stats["timings"]["svdag_traversal_time"])

def total_render_time(stats):
	return read_time_value(stats["timings"]["total_render_time"])

def culling_percentage(stats):
	return stats["svdag"]["num_rays_culled"]["value"] / stats["svdag"]["num_intersection_tests"]["value"] * 100

def svdag_memory_after_compression(stats):
	return stats["memory"]["svdags_after_compression"]["value"]

def svdag_memory_before_compression(stats):
	return stats["memory"]["svdags_before_compression"]["value"]


def plot_svdag_culling_percentage(stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(stats.items()):
		# Should be the same every run
		x = [svdag_resolution(runs[0]) for runs in scene_stats]
		y = [np.mean([culling_percentage(x) for x in runs])
                    for runs in scene_stats]
		ax.plot(x, y, label=scene, color=colors[i], **plot_style)
	
	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter('%d%%'))
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Batching points culled")
	#plt.xlim(0, 550)
	ax.set_ylim(0, 100)
	ax.set_xscale("log")
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_culling_percentage.pdf", bbox_inches="tight")
	#plt.show()



def plot_svdag_traversal_time(stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(stats.items()):
		x = [svdag_resolution(runs[0]) for runs in scene_stats]
		y = [np.mean([svdag_traversal_time(x) for x in runs])
                    for runs in scene_stats]
		stddev = [np.std([svdag_traversal_time(x) for x in runs])
                    for runs in scene_stats]
		ax.errorbar(x, y, yerr=stddev, label=scene,
		            color=colors[i], **errorbar_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("SVDAG traversal CPU time (s)")
	ax.set_ylim(bottom=0)
	ax.set_xscale("log")
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_traversal_time.pdf", bbox_inches="tight")
	#plt.show()

def plot_svdag_render_time(stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(stats.items()):
		x = [svdag_resolution(runs[0]) for runs in scene_stats]
		y = [np.mean([total_render_time(x) for x in runs]) for runs in scene_stats]
		stddev = [np.std([total_render_time(x) for x in runs])
                    for runs in scene_stats]
		ax.errorbar(x, y, yerr=stddev, label=scene,
		            color=colors[i], **errorbar_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Total render wall clock time (s)")
	ax.set_ylim(bottom=0)
	ax.set_xscale("log")
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_render_time.pdf", bbox_inches="tight")
	#plt.show()


def plot_svdag_memory(stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(stats.items()):
		x = [svdag_resolution(runs[0]) for runs in scene_stats]
		y = [np.mean([svdag_memory_after_compression(x) / 1000000000 for x in runs])
                    for runs in scene_stats]
		ax.plot(x, y, label=scene,
		            color=colors[i], **plot_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Memory usage (GB)")
	#ax.set_ylim(bottom=0)
	ax.set_xscale("linear")
	ax.set_yscale("linear")
	#ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter('%dGB'))
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_memory_usage.pdf", bbox_inches="tight")
	#plt.show()


def plot_svdag_compression(stats):
	# brewer2mpl.get_map args: set name  set type  number of colors
	bmap = brewer2mpl.get_map('Dark2', 'qualitative', 3)
	colors = bmap.mpl_colors

	fig = plt.figure(figsize=(8, 6))
	ax = fig.gca()
	for i, (scene, scene_stats) in enumerate(stats.items()):
		x = [svdag_resolution(runs[0]) for runs in scene_stats]
		y = [np.mean([svdag_memory_after_compression(x) / svdag_memory_before_compression(x) * 100 for x in runs])
                    for runs in scene_stats]
		ax.plot(x, y, label=scene, color=colors[i], **plot_style)

	ax.tick_params("both", length=10, width=1.5, which="major", direction="in")
	ax.tick_params("both", length=10, width=1, which="minor", direction="in")
	ax.yaxis.set_major_formatter(mpl.ticker.FormatStrFormatter('%d%%'))
	ax.set_xlabel("SVDAG resolution")
	ax.set_ylabel("Compression ratio")
	ax.set_ylim(0,100)
	ax.set_xscale("log")
	ax.legend(prop={"size": font_size}, frameon=False)
	fig.tight_layout()
	fig.savefig("svdag_compression.pdf", bbox_inches="tight")
	#plt.show()


def group_runs(stats):
	ret = {}
	for scene, scene_stats in stats.items():
		scene_stats.sort(key=lambda x: svdag_resolution(x))
		grouped_stats = [list(g) for k, g in itertools.groupby(
			scene_stats, svdag_resolution)]
		ret[scene] = grouped_stats
	return ret


if __name__ == "__main__":
	stats = discover_read_stat_files(
		"C:/Users/Mathijs/iCloudDrive/thesis/results_morton_order/svdag_perf")
	stats = group_runs(stats)
	
	#del stats["crown"]
	#del stats["landscape"]
	#del stats["island"]

	plot_svdag_culling_percentage(stats)
	plot_svdag_traversal_time(stats)
	plot_svdag_render_time(stats)
	plot_svdag_memory(stats)
	plot_svdag_compression(stats)
