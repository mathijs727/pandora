import ujson
import os


def read_stat_file(folder):
	with open(os.path.join(folder, "stats.json"), "r") as f:
		return ujson.load(f)[0]["data"]


def discover_read_stat_files(folder):
	results = {}
	test_folders = [os.path.join(folder, f) for f in os.listdir(folder) if os.path.isdir(os.path.join(folder, f))]
	for test_folder in test_folders:
		scene = os.path.basename(test_folder).split("_")[0]
		stats_filename = os.path.join(test_folder, "stats.json")
		if not os.path.exists(stats_filename):
			print(f"Warning: stats.json missing from {stats_filename}")
			continue

		with open(stats_filename, "r") as f:
			if scene in results:
				results[scene].append(ujson.load(f)[0]["data"])
			else:
				results[scene] = [ujson.load(f)[0]["data"]]

	return results


def read_time_value(stat):
	if stat["unit"] == "seconds":
		return stat["value"]
	elif stat["unit"] == "milliseconds":
		return stat["value"] / 1000
	elif stat["unit"] == "microseconds":
		return stat["value"] / 1000000
	elif stat["unit"] == "nanoseconds":
		return stat["value"] / 1000000000
	raise Exception("Unknown time unit " + stat["unit"])
