# Based on: https://github.com/mikkeloscar/arch-travis
import subprocess
import yaml
import os

seperator = "::::"
env_var_whitelist = ["CC", "CXX"]

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
    print(os.system("sudo docker pull mikkeloscar/arch-travis"))

    # run docker container
    #pwd = os.getcwd()
    project_dir = os.environ["TRAVIS_BUILD_DIR"]
    docker_cmd = "sudo docker run --rm -v {}:/build \
        -e CONFIG_REPOS=\"{}\" \
        -e CONFIG_PACKAGES=\"{}\" \
        -e CONFIG_BUILD_SCRIPTS=\"{}\" \
        {} \
        mikkeloscar/arch-travis".format(project_dir, config_repos, config_packages, config_build_scripts, env_vars_args)
    print(docker_cmd)
    return os.system(docker_cmd)
