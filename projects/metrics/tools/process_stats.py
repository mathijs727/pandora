import json
import matplotlib.pyplot as plt
from collections import namedtuple
from deepmerge import always_merger

Snapshot = namedtuple("Snapshot", ["timestamp", "data"])


def build_dictionary(file_name):
    message_list = []
    with open(file_name, "r") as file:
        def make_leafs_lists(dictionary):
            result = {}
            for key, value in dictionary.items():
                if isinstance(value, dict):
                    result[key] = make_leafs_lists(value)
                else:
                    if isinstance(value, int) or isinstance(value, float):
                        result[key] = [value]
                    else:
                        result[key] = value
            return result

        list_of_dicts = json.load(file)

        dict_of_lists = {}
        timestamps = []
        for snapshot in list_of_dicts:
            timestamp = snapshot["timestamp"]
            data = snapshot["data"]

            snapshot_dict_of_lists = make_leafs_lists(data)
            always_merger.merge(dict_of_lists, snapshot_dict_of_lists)

            timestamps.append(timestamp)

        return (timestamps, dict_of_lists)


def plot_group(timestamps, group, plot_title, unit):
    handles = []
    plt.title(plot_title)
    for name, metric in group.items():
        handle = plt.plot(timestamps, metric["value"], label=name)
        handles.append(handle)

    # plt.xlim(xmin=0)
    plt.ylabel(unit)
    plt.legend()
    plt.show()


if __name__ == "__main__":
    timestmaps, stats_dict = build_dictionary("stats.json")
    plot_group(timestmaps, stats_dict["memory"], "Memory usage", "MB")
