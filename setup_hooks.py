import os
import shutil
import stat


def add_python_hook(hook_name, additional_files=[]):
    hooks_folder = "./.git/hooks/"
    hooks_source_folder = "./hooks/"

    hook_path = os.path.join(hooks_folder, hook_name)
    hook_source_path = os.path.join(hooks_source_folder, hook_name)
    shutil.copyfile(hook_source_path, hook_path)
    os.chmod(hook_path, os.stat(hook_path).st_mode | stat.S_IXUSR |
             stat.S_IXGRP | stat.S_IXOTH)  # Add execution rights

    for filename in additional_files:
        source_path = os.path.join(hooks_source_folder, filename)
        dest_path = os.path.join(hooks_folder, filename)
        if os.path.exists(dest_path):
            os.remove(dest_path)
        shutil.copyfile(source_path, dest_path)


if __name__ == "__main__":
    add_python_hook("pre-commit",
                    ["pre_commit.py",
                     "pre_commit_clang_format.py",
                     "pre_commit_pep8.py"])
