import matplotlib.pyplot as plt
import numpy as np
import os
from helpers import discover_read_stat_files, read_time_value

result_folder = "E:/Windows/OneDrive/UU/Thesis/final/results/batching_point_size"

def batching_point_size(stats):
	return stats["ooc"]["prims_per_leaf"]

def num_batching_points(stats):
	return stats["ooc"]["num_top_leaf_nodes"]["value"]

def total_render_time(stats):
	return read_time_value(stats["timings"]["total_render_time"])

def plot_batching_point_render_time(stats):
	plt.title("Batching point size vs total render time (wall clock)")

	for scene, scene_stats in stats.items():
		if scene != "island":
			continue
		
		scene_stats.sort(key=lambda x: batching_point_size(x))
		x = [batching_point_size(test_stats) for test_stats in scene_stats]
		y = [total_render_time(test_stats) for test_stats in scene_stats]
		plt.plot(x, y, label=scene)

		plt.title("Batch point size vs total render time (wall seconds)")
		plt.xlabel("Batching point size")
		plt.ylabel("Render time (seconds)")
		plt.legend()
		plt.show()

		y = [num_batching_points(test_stats) for test_stats in scene_stats]
		plt.plot(x, y, label=scene)

		plt.title("Batch point size vs num batching points")
		plt.xlabel("Batching point size")
		plt.ylabel("Num batching points")
		plt.legend()
		plt.show()


if __name__ == "__main__":
	stats = discover_read_stat_files(result_folder)
	plot_batching_point_render_time(stats)
