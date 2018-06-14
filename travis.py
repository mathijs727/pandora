# Based on: https://github.com/mikkeloscar/arch-travis
import subprocess
import yaml
import os

seperator = "::::"
env_var_whitelist = ["TRAVIS_BUILD_DIR", "CC", "CXX"]

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
    env_vars_args = [["-e", f"{key}={value}"] for (key, value) in env_vars]
    env_vars_args = [item for sublist in env_vars_args for item in sublist]
    env_vars_args = " ".join(env_vars_args)

    # force pull latest
    print(os.system("docker pull mikkeloscar/arch-travis"))

    # run docker container
    pwd = os.getcwd()
    docker_cmd = f"sudo docker run --rm -v $(pwd):/build \
        -e CONFIG_REPOS=\"{config_repos}\" \
        -e CONFIG_PACKAGES=\"{config_packages}\" \
        -e CONFIG_BUILD_SCRIPTS=\"{config_build_scripts}\" \
        {env_vars_args} \
        mikkeloscar/arch-travis"
    print(docker_cmd)
    os.system(docker_cmd)
