import json
import plotly.graph_objects as go
import plotly.figure_factory as ff
import datetime
import numpy as np
import argparse


def flush_duration(flush):
    return flush["static_data_load_time"]["value"] + flush["processing_time"]["value"]


def create_flame_graph(stream_data):
    flushes = stream_stats[0]["data"]["flushes"]
    max_flush_time = np.max([flush_duration(f) for f in flushes])
    #task_names = set([t["task_name"] for t in flushes])

    print("Collecting data frame")
    df = []
    for flush in flushes:
        static_data_load_time_ns = flush["static_data_load_time"]["value"]
        processing_time_ns = flush["processing_time"]["value"]
        start_time_ns = flush["start_time"]
        finish_time_ns = start_time_ns + static_data_load_time_ns + processing_time_ns

        df.append({
            "Task": flush["task_name"],
            "Start": start_time_ns,
            "Finish": finish_time_ns,
            "Complete": (finish_time_ns - start_time_ns) / max_flush_time * 100
        })

    print("Creating figure")
    fig = ff.create_gantt(df, index_col="Complete",
                          showgrid_x=True, show_colorbar=True, group_tasks=True)
    fig.write_html("flamegraph.html", auto_open=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create flame graph of render tasks")
    parser.add_argument("--file", required=True, type=str, help="Path to stream stats file (stream_stats.json)")
    args = parser.parse_args()

    print("Loading file")
    with open(args.file) as f:
        stream_stats = json.load(f)

    create_flame_graph(stream_stats)
