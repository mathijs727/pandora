import argparse
import os
from klepto.archives import hdf_archive
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
    parser.add_argument("--file", type=str, required=True,
                        help="Either the PBRT scene file OR the intermediate file that was outputted on a previous run")
    parser.add_argument("--out", type=str, required=True,
                        help="Path/name of output Pandora scene description file")
    parser.add_argument("--intermediate", type=bool, required=False, default=True,
                        help="Whether to output intermediate files")
    args = parser.parse_args()

    if not os.path.isfile(args.file):
        print("Error: pbrt scene file not found")
        exit(1)
    if not os.path.exists(os.path.dirname(args.out)):
        print("Error: output path does not exist")
        exit(1)

    int_mesh_folder = os.path.join(os.path.dirname(args.out), "pbrt_meshes")
    out_mesh_folder = os.path.join(os.path.dirname(args.out), "pandora_meshes")

    if args.file.endswith(".pbrt"):
        print("==== PARSING PBRT FILE ====")
        pbrt_data = parsing.parse_file(args.file, int_mesh_folder)
    elif args.file.endswith(".bin"):
        #with open(args.file, "r") as f:
        #    pbrt_data = hickle.load(f)
        pbrt_data = hdf_archive(args.file, cached=False).load()
    else:
        print("Input file has unknown file extension")
        exit(-1)

    if args.intermediate:
        print("==== STORING INTERMEDIATE DATA TO A FILE ====")
        int_file = os.path.join(os.path.dirname(args.out), "intermediate.bin")
        hdf_archive(int_file, pbrt_data, cached=False)
        #with open(int_file, "w") as f:
        #    hickle.dump(pbrt_data, f)# Pickle crashes here on the Island scene with a memory error

    print("==== CONVERTING TO PANDORA FORMAT ====")
    pandora_data = extract_pandora_data(pbrt_data, out_mesh_folder)

    print("==== WRITING PANDORA SCENE FILE ====")
    ujson.dump(pandora_data, open(args.out, "w"), indent=2)

