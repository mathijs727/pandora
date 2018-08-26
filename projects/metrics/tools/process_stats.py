import json
from itertools import accumulate
from deepmerge import always_merger, merge_or_raise

def build_dictionary(file_name):
	message_list = []
	with open(file_name, "r") as file:
		message_list = json.load(file)

	metric_dict = {}
	for message in message_list:
		# https://stackoverflow.com/questions/40401886/how-to-create-a-nested-dictionary-from-a-list-in-python
		metric = message["identifier"][-1]
		groups = message["identifier"][:-1]
		
		message_content = message["content"]
		if message_content["type"] == "value_changed":
			partial_dict = {metric: [message_content["new_value"]]}
			for group in reversed(groups):
				partial_dict = {group: partial_dict}

		always_merger.merge(metric_dict, partial_dict)

	return metric_dict

if __name__ == "__main__":
	stats_dict = build_dictionary("stats.json")
	print(stats_dict)