import subprocess
import os
import difflib
import time
from datetime import datetime
from multiprocessing import Pool

python_extensions = [".py"]


def gen_pep8_diff(file_data):
    file_status = file_data[0]
    if file_status.endswith("D"):  # Deleted
        return ""

    filename = file_data[-1]
    if not filename.lower().endswith(tuple(python_extensions)):
        return ""

    # Run pep8 on the input file
    subprocess.check_output(["autopep8", "-a", "-a", "--in-place", filename])

    # Get the diff from the commited file to the pep8 formatted version
    diff = subprocess.check_output(
        ["git", "diff", filename]).decode("utf-8")

    if diff:
        # Undo pep8 formatting changes and restore the file back to how it was
        # when it got commited
        subprocess.check_output(["git", "checkout", "--", filename])

    return diff


def auto_pep8(files, patch_filename):
    # Process files in parallel
    pool = Pool()
    diff_results = pool.map(gen_pep8_diff, files)
    diff_results = filter(None, diff_results)  # Remove emptry strings
    diff_patch = "\n".join(diff_results)

    if not diff_patch:
        return True
    else:
        print("The following differences were found between the code to commit")
        print("and the pep8 rules:\n")
        print(diff_patch)

        # Write the diff patch to a file
        with open(patch_filename, "w") as file:
            for line in diff_patch.split("\n"):
                file.write(line + "\n")

        print("\nYou can apply these changes with:\n git apply %s\n" %
              patch_filename)
        print(
            "(may need to be called from the root directory of your repository)")
        print(
            "Aborting commit. Apply changes and commit again")
        print("###############################")
        return False
