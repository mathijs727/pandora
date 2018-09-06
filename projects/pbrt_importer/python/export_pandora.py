import argparse
import pickle
import ujson # ujson is way faster than the default json module for exporting large dictionaries
from config_parser import ConfigParser
from scene_parser import SceneParser
import os

def extract_pandora_data(pbrt_data, out_mesh_folder):
    if not os.path.exists(out_mesh_folder):
        os.makedirs(out_mesh_folder)

    config_data = ConfigParser(pbrt_data["config"])
    scene_data = SceneParser(pbrt_data["scene"], out_mesh_folder)
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

    out_mesh_folder = os.path.join(os.path.dirname(args.out), "pandora_meshes")

    pbrt_data = pickle.load(open(args.file, "rb"))
    pandora_data = extract_pandora_data(pbrt_data, out_mesh_folder)

    print("Writing JSON")
    ujson.dump(pandora_data, open(args.out, "w"), indent=2)


"""import pandora_py

if __name__ == "__main__":
    world = pandora_py.createWorld()
    #world.set("HELLO WORLD")
    print(world.greet())"""
