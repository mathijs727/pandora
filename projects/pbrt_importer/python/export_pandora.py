import argparse
import pickle
import json
from collections import namedtuple
from parsing.parser import Texture
import numpy as np


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
        self._float_textures = UniqueList()
        self._color_textures = UniqueList()

        self._create_scene_objects(pbrt_scene)
        self._create_scene_objects_instancing(pbrt_scene)

    def _create_texture_id(self, texture_info):
        tex_type = texture_info["type"]
        if tex_type == "float":
            # Inline texture property
            texture = Texture(type=tex_type, texture_class="constant", arguments={
                              "value": texture_info["value"]})
        elif tex_type == "spectrum" or tex_type == "color":
            # Inline texture property
            texture = Texture(type=tex_type, texture_class="constant", arguments={
                              "value": list(texture_info["value"])})
        elif tex_type == "texture":
            # Texture class
            texture = texture_info["value"]
            arguments = {}
            for key, value in texture.arguments.items():
                arguments[key] = value["value"]
            texture = Texture(texture.type, texture.texture_class, arguments)
        else:
            print(f"UNKNOWN TEXTURE: {texture_info}")
            exit(1)

        texture_dict = {
            "type": texture.type if texture.type != "spectrum" else "color",
            "class": texture.texture_class,
            "arguments": texture.arguments
        }

        if texture_dict["type"] == "float":
            return self._float_textures.add_item(texture_dict)
        else:
            return self._color_textures.add_item(texture_dict)

    def _create_material_id(self, material):
        if material.type == "matte":
            kd = self._create_texture_id(material.arguments["Kd"])
            sigma = self._create_texture_id(material.arguments["sigma"])

            # Leave out texture support for now?
            return self._materials.add_item({
                "type": "matte",
                "arguments": {
                    "kd": kd,
                    "sigma": sigma
                }
            })
        else:
            print(
                f"Material type {material.type} is not supported yet. Replacing by matte white placeholder.")

            kd = self._create_texture_id(
                {"type": "spectrum", "value": np.array([1.0, 1.0, 1.0])})
            sigma = self._create_texture_id({"type": "float", "value": 0.5})

            return self._materials.add_item({
                "type": "matte",
                "arguments": {
                    "kd": kd,
                    "sigma": sigma
                }
            })

    def _create_scene_objects(self, pbrt_scene):
        for shape in pbrt_scene["non_instanced_shapes"]:
            if shape.type == "plymesh":
                if shape.area_light is not None:
                    print(f"Area light: {shape.area_light}")

                geometry_id = self._geometry.add_item({
                    "type": "triangle",
                    "filename": shape.arguments["filename"]["value"],
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
            "geometry": self._geometry.get_list(),
            "scene_objects": self._scene_objects.get_list(),
            "materials": self._materials.get_list(),
            "float_textures": self._float_textures.get_list(),
            "color_textures": self._color_textures.get_list()
        }


def extract_pandora_data(pbrt_data):
    # Make up for typo in the PBRT parser (fixed now)
    pbrt_scene_data = pbrt_data["scene"]
    if "non_instances_shapes" in pbrt_scene_data:
        pbrt_scene_data["non_instanced_shapes"] = pbrt_scene_data["non_instances_shapes"]
        del pbrt_scene_data["non_instances_shapes"]

    scene_data = SceneParser(pbrt_scene_data)
    return scene_data.data()


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
