import subprocess
import os
import re
import time
import shared_benchmark_code


result_output_folder = "C:/Users/Mathijs/OneDrive/TU Delft/Projects/Batched Ray Traversal/Results/"

def run_pandora_with_defaults(scene, out_folder, geom_cache, bvh_cache, svdag_res):
	args = [
		"--file", scene["file"],
		"--subdiv", scene["subdiv"],
		"--cameraid", 0,
		"--integrator", "path",
		"--spp", 256,
		"--concurrency", 4000000,
		"--schedulers", 8,
		"--geomcache", geom_cache,
		"--bvhcache", bvh_cache,
		"--primgroup", scene["batch_point_size"],
		"--svdagres", svdag_res
	]
	shared_benchmark_code.run_pandora(args, out_folder)


def test_svdag_no_mem_limit(scenes, num_runs = 1):
	for scene in scenes:
		for svdag_res in [128, 64, 256, 512]:
			for run in range(1, num_runs+1):
				formatted_time =  time.asctime(time.localtime(time.time())) # https://www.tutorialspoint.com/python3/python_date_time.htm
				print(f"Current time: {formatted_time}")
				print(f"Benchmarking {scene['name']} at SVDAG res {svdag_res} (run {run})\n")
				out_folder = os.path.join(result_output_folder, "svdag_res", str(svdag_res), scene["name"], f"run-{run}")
				run_pandora_with_defaults(
					scene,
					out_folder,
					1000*1000, # Unlimited geom cache
					1000*1000, # Unlimited BVH cache
					svdag_res)


# Test as memory limit (1.0 = full memory, 0.5 = 50%, etc...)
def test_at_memory_limit(scenes, geom_memory_limit, bvh_memory_limit, culling, num_runs = 1):
	for scene in scenes:
		for run in range(num_runs):
			mem_limit_percentage = int(geom_memory_limit * 100)
			culling_str = "culling" if culling else "no-culling"

			formatted_time =  time.asctime(time.localtime(time.time())) # https://www.tutorialspoint.com/python3/python_date_time.htm
			print(f"Current time: {formatted_time}")
			print(f"Benchmarking {scene['name']} at memory limit {mem_limit_percentage} with {culling_str} (run {run})\n")
			out_folder = os.path.join(result_output_folder, "mem_limit", str(mem_limit_percentage), culling_str, scene["name"], f"run-{run}")

			# Adjust for the memory used by the SVDAGs. Equally spread this "cost" over both geometry & BVH cache.
			if culling:
				svdag_mem_usage_mb = scene["svdag_mem_usage128"] / 1000000
			else:
				svdag_mem_usage_mb = 0
			geom_mem_mb = int(scene["max_geom_mem_mb"] * geom_memory_limit - svdag_mem_usage_mb / 2)
			bvh_mem_mb = int(scene["max_bvh_mem_mb"] * bvh_memory_limit)
			run_pandora_with_defaults(
				scene,
				out_folder,
				geom_mem_mb,
				bvh_mem_mb,
				128 if culling else 0)

if __name__ == "__main__":
	scenes = shared_benchmark_code.get_scenes()
	#test_svdag_no_mem_limit(scenes, 1)
	
	for i in [1.0, 0.9, 0.8, 0.7, 0.6]:
		test_at_memory_limit(scenes, i, 1.0, False, 1)
		test_at_memory_limit(scenes, i, 1.0, True, 1)