import argparse
import pickle
import json
from collections import namedtuple
from parsing.parser_basics import Point, Point2, Normal, Noraml2, Vector, Vector2, RGB, XYZ, SampledSpectrum, SampledSpectrumFile, TextureRef, BlackBody

class UniqueList:
    def __init__(self):
        self._list = []

    def add_item(self, item):
        # TODO: look for duplicates
        index = len(self._list)
        self._list.append(item)
        return index

    def get_list(self):
        return self._list


class SceneParser:
    def __init__(self, pbrt_scene):
        self._geometry = UniqueList()
        self._scene_objects = UniqueList()
        self._materials = UniqueList()
        self._textures = UniqueList()

        self._create_scene_objects(pbrt_scene)
        self._create_scene_objects_instancing(pbrt_scene)

    def _create_texture_id(self, value):
        if isinstance(value, dict):
            assert("filename" in value)
            texture = {
                "type": "image",
                "filename": value["filename"]
            }
        elif isinstance(value, RGB):
            texture = {
                "type": "constant",
                "dim": len(value),
                "values": [float(value.x), float(value.y), float(value.z)]
            }
        else:
            texture = {
                "type": "constant",
                "dim": 1,
                "value": float(value)
            }

        return self._textures.add_item(texture)

    def _create_material_id(self, material):
        print(material)
        supported_materials = ["matte"]
        if material.type == "matte":
            kd = self._create_texture_id(material.arguments["Kd"])
            sigma = self._create_texture_id(material.arguments["sigma"])

            # Leave out texture support for now?
            return self._materials.add_item({
                "type": "matte",
                "kd_tex_id": kd,
                "sigma_tex_id": sigma
            })
        else:
            print(
                f"Material type {material.type} is not supported yet. Replacing by matte white placeholder.")

            kd = self._create_texture_id([1.0, 1.0, 1.0])
            sigma = self._create_texture_id(0.5)

            return self._materials.add_item({
                "type": "matte",
                "kd": kd,
                "sigma": sigma
            })

    def _create_scene_objects(self, pbrt_scene):
        for shape in pbrt_scene["non_instanced_shapes"]:
            print(shape)

            if shape.type == "plymesh":
                if shape.area_light is not None:
                    print(f"Area light: {shape.area_light}")
                
                geometry_id = self._geometry.add_item({
                    "type": "triangle",
                    "filename": shape.arguments["filename"],
                    "transform": shape.transform
                })
                material_id = self._create_material_id(shape.material)

                self._scene_objects.add_item({
                    "geometry_id": geometry_id,
                    "material_id": material_id
                })

            else:
                continue

    def _create_scene_objects_instancing(self, pbrt_scene):
        pass

    def _shape_to_obj(self, shape):
        pass

    def data(self):
        return {
            "geometry": self._geometry,
            "scene_objects": self._scene_objects,
            "materials": self._materials,
        }


def extract_pandora_data(pbrt_data):
    # Make up for typo in the PBRT parser (fixed now)
    pbrt_scene_data = pbrt_data["scene"]
    if "non_instances_shapes" in pbrt_scene_data:
        pbrt_scene_data["non_instanced_shapes"] = pbrt_scene_data["non_instances_shapes"]
        del pbrt_scene_data["non_instances_shapes"]

    scene_data = SceneParser(pbrt_scene_data)

    return scene_data


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
