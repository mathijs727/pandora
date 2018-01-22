import os
import sys
import subprocess
import json
import multiprocessing
import shutil
import tarfile
import zipfile

# Python 2 and 3: alternative 4
try:
    from urllib.parse import urlsplit
    from urllib.request import urlretrieve
except ImportError:
    from urlparse import urlsplit
    from urllib import urlretrieve

script_path = os.path.dirname(os.path.abspath(__file__))
dependencies_file = "dependencies.json"
downloads_folder = os.path.join(script_path, "downloads")
installs_folder = os.path.join(script_path, "install")


def monkey_patch_tarfile():
    # Fix max file path length errors in tarfile
    # https://stackoverflow.com/questions/4677234/python-ioerror-exception-when-creating-a-long-file
    import os
    import sys
    if sys.platform not in ['cygwin', 'win32', 'win64']:
        return

    def long_open(name, *args, **kwargs):
        # http://msdn.microsoft.com/en-us/library/aa365247%28v=vs.85%29.aspx#maxpath
        if len(name) >= 200:
            if not os.path.isabs(name):
                name = os.path.join(os.getcwd(), name)
            name = "\\\\?\\" + os.path.normpath(name)
        return long_open.bltn_open(name, *args, **kwargs)
    long_open.bltn_open = tarfile.bltn_open
    tarfile.bltn_open = long_open


def find_dependency_data(dep_name):
    dependencies = json.load(open(dependencies_file))

    try:
        return dependencies[dep_name]
    except KeyError:
        return None


def get_for_platform(dep_data, key):
    try:
        return dep_data["all"][key]
    except KeyError:
        if sys.platform == "win32" or sys.platform == "win64":
            platform = "windows"
        else:
            platform = "unix"
        return dep_data[platform][key]


def download_dependency(dep_name, dep_data):
    download_folder = os.path.join(downloads_folder, dep_name)
    download_data = get_for_platform(dep_data, "download")

    # Folder already exists (we downloaded it before)
    if os.path.exists(download_folder):
        return True
    else:
        one_folder_up = os.path.dirname(download_folder)
        if not os.path.exists(one_folder_up):
            os.makedirs(one_folder_up)

    if "git" in download_data:
        subprocess.check_call(
            ["git", "clone", download_data["git"], download_folder])

    if "url" in download_data:  # Download and decompress zip/tar files
        # https://stackoverflow.com/questions/2795331/python-download-without-supplying-a-filename
        filename = urlsplit(
            download_data["url"]).path.split("/")[-1]
        file_path = os.path.join(downloads_folder, filename)

        if not os.path.exists(file_path):
            urlretrieve(download_data["url"], file_path)

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

            # exit()

            if "unpack_folder" in download_data:
                old_path = os.path.join(
                    tmp_path, download_data["unpack_folder"])
                shutil.move(old_path, download_folder)
                #shutil.copytree(old_path, download_folder)
                #print("Move from: %s to %s" % (old_path, download_folder))
            else:
                shutil.move(tmp_path, download_folder)

            # Delete the folder to which we unpacked the zip/tar file
            os.rmdir(tmp_path)

        else:
            print("Cannot uncompress unknown format: %s" % filename)

        os.remove(file_path)  # Delete the zip/tar file

    return True


def install_dependency(dep_name, dep_data, build_type):
    build_data = get_for_platform(dep_data, "build")
    if "single_threaded" in build_data and build_data["single_threaded"]:
        core_count = 1
    else:
        core_count = multiprocessing.cpu_count()
    install_folder = os.path.join(
        installs_folder, build_type.lower(), dep_name)
    if os.path.exists(install_folder):
        return True  # Already exists
    else:
        one_folder_up = os.path.dirname(install_folder)
        if not os.path.exists(one_folder_up):
            os.makedirs(one_folder_up)

    if build_data["build_system"] == "CMake":
        download_folder = os.path.join(downloads_folder, dep_name)
        # Cant be "build" because that clashes with GLEW
        build_folder = os.path.join(
            download_folder,
            "pandora_build_%s" %
            build_type)

        try:
            # Clean the build folder
            if os.path.exists(build_folder):
                shutil.rmtree(build_folder)
            os.makedirs(build_folder)

            try:
                cmake_flags = build_data["flags"]
            except KeyError:
                cmake_flags = []

            if "cmakelists_folder" in build_data:
                path_to_cmakelists = os.path.join(
                    "../", build_data["cmakelists_folder"])
            else:
                path_to_cmakelists = "../"

            # Multithreading
            generator_flags = []
            if generator == "Ninja" or generator == "Unix Makefiles":
                generator_flags.append("-j%d" % core_count)

            # Force 64 bit compilation
            # TODO: build dependencies for both 32 and 64 bit
            if generator.startswith("Visual Studio"):
                cmake_flags.append("-DCMAKE_GENERATOR_PLATFORM=x64")
            subprocess.check_output(
                [
                    "cmake",
                    "-DCMAKE_BUILD_TYPE=%s" % build_type,
                    "-G%s" %
                    generator,
                    "-DCMAKE_INSTALL_PREFIX=%s" % install_folder,
                    path_to_cmakelists] +
                cmake_flags,
                cwd=build_folder)
            subprocess.check_call(["cmake",
                                   "--build",
                                   ".",
                                   "--target",
                                   "install",
                                   "--config",
                                   build_type,
                                   "--"] + generator_flags,
                                  cwd=build_folder)
            return True
        except Exception as e:
            print("Failed to build using this CMake")
            print(e)

    elif build_data["build_system"] == "boost":
        download_folder = os.path.join(downloads_folder, dep_name)

        # Build the Boost.build build system
        if sys.platform == "win32" or sys.platform == "win64":  # If Windows
            # Bug that has been fixed on the master branch of Boost.build:
            # https://github.com/boostorg/build/pull/265
            build_jam = os.path.join(
                download_folder,
                "tools",
                "build",
                "src",
                "engine",
                "build.jam")
            with open(build_jam, "r") as file:
                txt = file.readlines()
            txt[186] = "toolset cc \"$(CC)\" : \"-o \" : -D"
            with open(build_jam, "w") as file:
                file.writelines(txt)

            bootstrap_path = os.path.join(download_folder, "bootstrap.bat")
        else:
            bootstrap_path = os.path.join(download_folder, "bootstrap.sh")
        subprocess.check_call([bootstrap_path], cwd=download_folder)

        if "with_libraries" in build_data:
            with_libraries = ["--with-%s" %
                              lib for lib in build_data["with_libraries"]]
        else:
            with_libraries = []

        try:
            b2_path = os.path.join(download_folder, "b2")
            result = subprocess.check_output([b2_path,
                                              build_type.lower(),
                                              "address-model=64",
                                              "threading=multi",
                                              "-j%d" % core_count,
                                              "-d0",  # Supress all informational messages
                                              "install",
                                              "--prefix=%s" % install_folder] + with_libraries, cwd=download_folder)
            result = result.decode("utf-8")
            print(result)
            return True
        except Exception as e:
            print("Failed to build using Boost.build")
            print(e)

    elif build_data["build_system"] == "none":
        return True

    return False


if __name__ == "__main__":
    # Patch tarfile so it can handle long paths on Windows
    monkey_patch_tarfile()

    if len(sys.argv) != 4:
        print("Script has three argument:")
        print(" - Name of the dependency")
        print(" - Generator")
        print(" - Build type (release or debug)")
        print("\nGiven: ", sys.argv)
        exit(1)

    build_type_lookup = {
        "debug": "Debug",
        "release": "Release",
        "relwithdebinfo": "Release",
        "minsizerel": "Release"
    }

    dep_name = sys.argv[1]
    generator = sys.argv[2]
    build_type = build_type_lookup[sys.argv[3].lower()]

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
    if not install_dependency(dep_name, dependency, build_type):
        print("Install failed")
    else:
        print("Install succeeded")

    exit(0)
