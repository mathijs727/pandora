import sys
import os
import subprocess

from pre_commit_clang_format import clang_format
from pre_commit_pep8 import auto_pep8

if __name__ == "__main__":
    print(os.path.abspath(os.path.dirname(sys.argv[0])))
    print("Running pre-commit script")

    patch_filename = "pre-commit-patch.patch"

    # Delete previous patch file (so we dont try to commit it)
    if os.path.exists(patch_filename):
        os.remove(patch_filename)
        try:
            # Remove patch from git
            subprocess.check_output(["git", "add", patch_filename])
        except:
            pass  # May fail if it was never commited

    # Get a list of files that changed
    text = subprocess.check_output(
        ["git", "status", "--porcelain", "-uno"],
        stderr=subprocess.STDOUT).decode("utf-8")
    file_list = [line.split(' ') for line in text.splitlines()]

    if not clang_format(file_list, patch_filename):
        sys.exit(1)  # Failure

    if not auto_pep8(file_list, patch_filename):
        sys.exit(1)  # Failure

    sys.exit(0)  # Success
