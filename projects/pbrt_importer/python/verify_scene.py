import os
import argparse
import ujson
import numpy as np

def verify_instance_references(pandora_scene):
    scene_info = pandora_scene["scene"]
    base_scene_objects = scene_info["instance_base_scene_objects"]

    base_obj_ref_count = np.zeros((len(base_scene_objects)))
    for scene_object in scene_info["scene_objects"]:
        if scene_object["instancing"]:
            base_obj_ref_count[scene_object["base_scene_object_id"]] += 1

    num_base_objects = len(base_obj_ref_count)
    unreferenced_base_objects = num_base_objects - \
        np.count_nonzero(base_obj_ref_count)
    print(
        f"Unreferenced base objects: {unreferenced_base_objects}/{num_base_objects}")
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Export preprocessed PBRT scenes to Pandora's JSON format")
    parser.add_argument("--scene", type=str, required=True,
                        help="Pandora scene (*json) file")
    args = parser.parse_args()

    print("Start loading scene json")
    with open(args.scene, "r") as f:
        pandora_scene = ujson.load(f)
    print("Done loading scene json")

    verify_instance_references(pandora_scene)

    


