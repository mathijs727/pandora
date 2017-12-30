import subprocess
import shutil
import os
import sys

if __name__ == "__main__":
    output_folder = sys.argv[1]

    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    for folder in sys.argv[2:]:
        for file in [
            os.path.join(
                folder,
                f) for f in os.listdir(folder) if os.path.isfile(
                os.path.join(
                folder,
                f))]:
            shutil.copy(file, output_folder)
