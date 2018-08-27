import json
import matplotlib.pyplot as plt
from collections import namedtuple
from deepmerge import always_merger
import numpy as np

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
                    # if isinstance(value, int) or isinstance(value, float):
                    if key == "value":
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


def plot_group(timestamps, group, plot_title):
    unit = list(group.values())[0]["unit"]
    plt.title(plot_title)
    for name, metric in group.items():
        # All items in the group should have the same unit
        assert(metric["unit"] == unit)
        plt.plot(timestamps, metric["value"], label=name)

    # plt.xlim(xmin=0)
    plt.ylabel(unit)
    plt.legend()
    plt.show()


def plot_histogram(metric, plot_title):
    assert(metric["type"] == "histogram")

    step_size = (metric["max"] - metric["min"]) / (metric["numBins"] - 2)
    half_step_size = step_size / 2

    bar_positions = np.linspace(
        metric["min"] - half_step_size, metric["max"] + half_step_size, metric["numBins"])
    values = metric["value"][-1]

    plt.title(plot_title)
    plt.bar(bar_positions, values, 0.8 * step_size)
    plt.xticks(np.linspace(metric["min"], metric["max"], metric["numBins"] - 1))

    # plt.xlim(xmin=0)
    # plt.ylabel(metric["unit"])
    plt.legend()
    plt.show()


if __name__ == "__main__":
    timestmaps, stats_dict = build_dictionary("stats.json")
    print(stats_dict)
    plot_histogram(stats_dict["histogram"], "Histogram")
    #plot_group(timestmaps, stats_dict["memory"], "Memory usage")
