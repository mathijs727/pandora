import matplotlib.pyplot as plt
import numpy as np
import os
from helpers import read_stat_file, read_time_value
import itertools
import argparse


def parallel_paths(stats):
	return stats["config"]["parallel_paths"]


def total_render_time(stats):
	return read_time_value(stats["timings"]["total_render_time"])


def plot_parallel_paths(stats):
	plt.title("Total render time (wall clock)")

	ax = plt.gca()
	for scene, scene_stats in stats.items():
		x = [parallel_paths(runs[0]) for runs in scene_stats]
		y = [np.mean([total_render_time(x) for x in runs]) for runs in scene_stats]
		stddev = [np.std([total_render_time(x) for x in runs])
						 for runs in scene_stats]
		ax.errorbar(x, y, yerr=stddev, label=scene)

	plt.xlabel("Parallel paths")
	plt.ylabel("Total render time (seconds)")
	plt.ylim(bottom=0)
	plt.legend()
	plt.show()


def group_runs(stats):
	ret = {}
	for scene, scene_stats in stats.items():
		scene_stats.sort(key=lambda x: parallel_paths(x))
		grouped_stats = [list(g) for k, g in itertools.groupby(
			scene_stats, parallel_paths)]
		ret[scene] = grouped_stats
	return ret


if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("--folder", type=str, help="Folder containing statistics")
	args = parser.parse_args()

	# stats = discover_read_stat_files(
	#	"E:/Windows/OneDrive/UU/Thesis/final/results/svdag_perf")
	# test_folder = "E:/Windows/OneDrive/UU/Thesis/final/results/parallel_paths"

	stats = {}
	for scene in ["crown", "sanmiguel", "island"]:
		scene_stats = []
		for par_paths in [2, 4, 8, 16, 24, 32]:
			for run in range(3):
				folder_name = f"{scene}_paralle_path_perf_{par_paths}_run{run}"
				test_stats = read_stat_file(os.path.join(args.folder, folder_name))
				test_stats["config"]["parallel_paths"] = par_paths * 1000000
				scene_stats.append(test_stats)
		stats[scene] = scene_stats


	stats = group_runs(stats)
	plot_parallel_paths(stats)
