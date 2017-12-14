import os
import sys
import subprocess
import json
import multiprocessing
import shutil
import urllib.request
import urllib.parse
import tarfile
import zipfile

script_path = os.path.dirname(os.path.abspath(__file__))
dependencies_file = "dependencies.json"
downloads_folder = os.path.join(script_path, "downloads")
installs_folder = os.path.join(script_path, "install")


def find_dependency_data(dep_name):
    dependencies = json.load(open(dependencies_file))

    try:
        return dependencies[dep_name]
    except KeyError:
        return None


def download_dependency(dep_name, dep_data):
    download_folder = os.path.join(downloads_folder, dep_name)

    for download_data in dep_data["download"]:
        # Folder already exists (we downloaded it before)
        if os.path.exists(download_folder):
            return True

        if "git" in download_data:
            subprocess.check_call(
                ["git", "clone", download_data["git"], download_folder])
            continue

        if "url" in download_data:
            # https://stackoverflow.com/questions/2795331/python-download-without-supplying-a-filename
            filename = urllib.parse.urlsplit(
                download_data["url"]).path.split("/")[-1]
            file_path = os.path.join(downloads_folder, filename)

            if not os.path.exists(file_path):
                urllib.request.urlretrieve(download_data["url"], file_path)

            # https://stackoverflow.com/questions/30887979/i-want-to-create-a-script-for-unzip-tar-gz-file-via-python
            if filename.endswith("tar.gz"):
                # Because Python is retarded and cant move the content of
                # folder to the parent folder
                tmp_path = os.path.join(downloads_folder, "tmp")

                tar = tarfile.open(file_path, "r:gz")
                tar.extractall(path=tmp_path)
                tar.close()

                if "unpack_folder" in download_data:
                    old_path = os.path.join(
                        tmp_path, download_data["unpack_folder"])
                    shutil.move(old_path, download_folder)
                else:
                    shutil.move(tmp_path, download_folder)

                # Delete the folder to which we unpacked the zip/tar file
                os.rmdir(tmp_path)
            elif filename.endswith("zip"):
                # Because Python is retarded and cant move the content of
                # folder to the parent folder
                tmp_path = os.path.join(downloads_folder, "tmp")

                zip_file = zipfile.ZipFile(file_path)
                zip_file.extractall(path=tmp_path)
                zip_file.close()

                if "unpack_folder" in download_data:
                    old_path = os.path.join(
                        tmp_path, download_data["unpack_folder"])
                    shutil.move(old_path, download_folder)
                else:
                    shutil.move(tmp_path, download_folder)

                # Delete the folder to which we unpacked the zip/tar file
                os.rmdir(tmp_path)

            else:
                print("Cannot uncompress unknown format: %s" % filename)

            os.remove(file_path)  # Delete the zip/tar file

    return True


def install_dependency(dep_name, dep_data):
    build_data = dep_data["build"]
    core_count = multiprocessing.cpu_count()
    install_folder = os.path.join(installs_folder, dep_name)
    if os.path.exists(install_folder):
        return True  # Already exists

    if build_data["build_system"] == "CMake":
        download_folder = os.path.join(downloads_folder, dep_name)
        # Cant be "build" because that clashes with GLEW
        build_folder = os.path.join(download_folder, "pandora_build")

        try:
            # Clean the build folder
            if os.path.exists(build_folder):
                shutil.rmtree(build_folder)
            os.makedirs(build_folder)

            try:
                build_flags = build_data["flags"]
            except KeyError:
                build_flags = []

            if "cmakelists_folder" in build_data:
                path_to_cmakelists = os.path.join(
                    "../", build_data["cmakelists_folder"])
            else:
                path_to_cmakelists = "../"
            subprocess.check_call(["cmake",
                                   "-GNinja",
                                   "-DCMAKE_INSTALL_PREFIX=%s" % install_folder,
                                   path_to_cmakelists] + build_flags,
                                  cwd=build_folder)
            subprocess.check_call(
                ["ninja", "-j%d" % core_count], cwd=build_folder)
            subprocess.check_call(["ninja", "install"], cwd=build_folder)
            return True
        except Exception as e:
            print("Failed to build using this CMake")
            print(e)

    elif build_data["build_system"] == "boost":
        download_folder = os.path.join(downloads_folder, dep_name)

        if os.name == "nt":  # If Windows
            bootstrap_path = os.path.join(download_folder, "bootstrap.bat")
        else:
            bootstrap_path = os.path.join(download_folder, "bootstrap.sh")
        subprocess.check_call([bootstrap_path], cwd=download_folder)

        b2_path = os.path.join(download_folder, "b2")
        try:
            subprocess.check_call(
                [b2_path, "install", "--prefix=%s" % install_folder], cwd=download_folder)
        except Exception as e:
            print(e)

    return False


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Script has one argument: name of the dependency")
        exit(1)

    dep_name = sys.argv[1]

    dependency = find_dependency_data(dep_name)
    if dependency is None:
        print(
            "Could not find the dependency. Enter its download and install information in {0}." %
            dependencies_file)
        exit(1)

    print("Downloading [%s]" % dep_name)
    if not download_dependency(dep_name, dependency):
        print("Could not download dependency")
        exit(1)

    print("Building [%s]" % dep_name)
    if not install_dependency(dep_name, dependency):
        print("Install failed")
    else:
        print("Install succeeded")
