# Based on: https://github.com/mikkeloscar/arch-travis
import subprocess
import yaml
import os
import re
from fix_dependencies import fix_eastl, fix_tinyply

seperator = "::::"
env_var_whitelist = ["CC", "CXX"]

if __name__ == "__main__":
    fix_eastl()
    fix_tinyply()

    with open(".travis.yml") as file:
        travis_config = yaml.load(file.read())
        config_build_scripts = travis_config["arch"]["script"]
        config_packages = travis_config["arch"]["packages"]
        #config_packages = None
        config_repos = travis_config["arch"]["repos"]

        config_build_scripts = seperator.join(
            config_build_scripts) if config_build_scripts is not None else ""
        config_packages = seperator.join(
            config_packages) if config_packages is not None else ""
        config_repos = seperator.join(
            config_repos) if config_repos is not None else ""

        env_vars = [(key, os.environ[key]) for key in env_var_whitelist]
        env_vars_args = [["-e", key + "=" + value] for (key, value) in env_vars]
        env_vars_args = [item for sublist in env_vars_args for item in sublist]
        env_vars_args = " ".join(env_vars_args)

        # force pull latest
        os.system("sudo docker pull mikkeloscar/arch-travis")
        
        # run docker container
        project_dir = os.environ["TRAVIS_BUILD_DIR"]
        docker_cmd = "sudo docker run --rm -v {}:/build \
            -e CONFIG_REPOS=\"{}\" \
            -e CONFIG_PACKAGES=\"{}\" \
            -e CONFIG_BUILD_SCRIPTS=\"{}\" \
            {} \
            mikkeloscar/arch-travis".format(project_dir, config_repos, config_packages, config_build_scripts, env_vars_args)
        result = os.popen(docker_cmd).read()

        result = str(result)# Bytes to string
        result = result.replace("\\n", "\n").replace("\\t", "\t")

        print(result + "\n\n")

        # nlohmann-json build from AUR also runs CMake tests, make sure to check only Pandoras tests
        matches = re.findall("([0-9]+)% tests passed", result)
        if len(matches) == 2 and matches[1] == "100":
            print("Success!")
            exit(0)

        print("Failed!")
        exit(-1)
