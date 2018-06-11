import os
import json
from external_dependencies import dependencies_file, installs_folder

def install_dependency(dependency_name, generator, build_type):
	#result = subprocess.check_output(["python", "external_dependencies.py", dependency_name, generator, build_type])
	#result = result.decode("utf-8")
	#print(result)
	print(f"Installing {dependency_name} {generator}")
	args = ["python", "external_dependencies.py", dependency_name, generator, build_type]
	os.system(" ".join([arg if " " not in arg else "\"%s\"" % arg for arg in args]))
	print(" ")


def get_all_dependencies():
	dependencies = json.load(open(dependencies_file))
	return dependencies.keys()


if __name__ == "__main__":
	generator = "Visual Studio 15 2017"

	dependency_names = get_all_dependencies()
	for dependency_name in dependency_names:
		install_dependency(dependency_name, generator, "debug")
		install_dependency(dependency_name, generator, "release")

	path = os.path.join(installs_folder, "debug", "GLEW", "lib")
	if os.path.exists(os.path.join(path, "libglew32d.lib")):
		os.rename(os.path.join(path, "libglew32d.lib"), os.path.join(path, "glew32.lib"))

	path = os.path.join(installs_folder, "release", "GLEW", "lib")
	if os.path.exists(os.path.join(path, "libglew32.lib")):
		os.rename(os.path.join(path, "libglew32.lib"), os.path.join(path, "glew32.lib"))

	path = os.path.join(installs_folder, "debug", "assimp", "lib")
	if os.path.exists(os.path.join(path, "assimp-vc140-mtd.lib")):
		os.rename(os.path.join(path, "assimp-vc140-mtd.lib"), os.path.join(path, "assimp-vc140-mt.lib"))
