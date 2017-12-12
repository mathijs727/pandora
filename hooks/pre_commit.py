import sys
import os
import subprocess

from pre_commit_clang_format import clang_format
from pre_commit_pep8 import auto_pep8

patch_filename = "pre-commit-patch.patch"
exceptions = ["external/tbb/"]

if __name__ == "__main__":
    print(os.path.abspath(os.path.dirname(sys.argv[0])))
    print("Running pre-commit script")

    # Delete previous patch file (so we dont try to commit it)
    if os.path.exists(patch_filename):
        os.remove(patch_filename)
        try:
            # Remove patch from git
            subprocess.check_output(["git", "add", patch_filename])
        except BaseException:
            pass  # May fail if it was never commited

    # Get a list of files that changed
    text = subprocess.check_output(
        ["git", "status", "--porcelain", "-uno"],
        stderr=subprocess.STDOUT).decode("utf-8")
    file_list = [line.split(' ') for line in text.splitlines()]

    # Skip files that are in the list of exceptions (files that do not need to be
    #  processed by this script). This is usefull for external dependencies.
    file_list = [file for file in file_list if not any(
        [e in file[-1] for e in exceptions])]

    if not clang_format(file_list, patch_filename):
        sys.exit(1)  # Failure

    if not auto_pep8(file_list, patch_filename):
        sys.exit(1)  # Failure

    sys.exit(0)  # Success
