import pathlib
import os

if __name__ == "__main__":
    folder = "C:/Users/Mathijs/iCloudDrive/thesis/results_morton_order/ooc_culling128 - Copy"
    remove_files_matching = ["*.jpg", "*.exr", "*.txt"]

    for match in remove_files_matching:
        for file_path in pathlib.Path(folder).glob(f"**/{match}"):
            os.remove(file_path)
            #print(file_path)
