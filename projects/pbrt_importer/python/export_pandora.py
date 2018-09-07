import argparse
import os
import parsing
from config_parser import ConfigParser
from scene_parser import SceneParser
import ujson  # ujson is way faster than the default json module for exporting large dictionaries


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
                        help="Path to PBRT scene file")
    parser.add_argument("--out", type=str,
                        help="Name/path of output Pandora scene description file")
    args = parser.parse_args()

    if not os.path.isfile(args.file):
        print("Error: pbrt scene file not found")
        exit(1)
    if not os.path.exists(os.path.dirname(args.out)):
        print("Error: output path does not exist")
        exit(1)

    out_mesh_folder = os.path.join(os.path.dirname(args.out), "pandora_meshes")

    #pbrt_data = pickle.load(open(args.file, "rb"))
    print("==== PARSING PBRT FILE ====")
    pbrt_data = parsing.parse_file(args.file)

    print("==== CONVERTING TO PANDORA FORMAT ====")
    pandora_data = extract_pandora_data(pbrt_data, out_mesh_folder)

    print("==== WRITING PANDORA SCENE FILE ====")
    ujson.dump(pandora_data, open(args.out, "w"), indent=2)


"""import pandora_py

if __name__ == "__main__":
    world = pandora_py.createWorld()
    #world.set("HELLO WORLD")
    print(world.greet())"""
