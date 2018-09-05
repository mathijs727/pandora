import argparse
import pickle
import json
from config_parser import ConfigParser
from scene_parser import SceneParser

def extract_pandora_data(pbrt_data):
    config_data = ConfigParser(pbrt_data["config"])
    scene_data = SceneParser(pbrt_data["scene"])
    return {
        "config": config_data.data(),
        "scene": scene_data.data()
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Export preprocessed PBRT scenes to Pandora's JSON format")
    parser.add_argument("--file", type=str,
                        help="Path to preprocessed PBRT scene (binary) file")
    parser.add_argument("--out", type=str,
                        help="Name of output scene description file")
    args = parser.parse_args()

    pbrt_data = pickle.load(open(args.file, "rb"))

    pandora_data = extract_pandora_data(pbrt_data)
    json.dump(pandora_data, open(args.out, "w"), indent=2)


"""import pandora_py

if __name__ == "__main__":
    world = pandora_py.createWorld()
    #world.set("HELLO WORLD")
    print(world.greet())"""
