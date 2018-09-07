from parsing.parser import Texture
from unique_collection import UniqueCollection
import numpy as np
import pickle
import os


class SceneParser:
    def __init__(self, pbrt_scene, out_mesh_folder):
        self._geometry = UniqueCollection()
        self._scene_objects = UniqueCollection()
        self._instance_base_scene_objects = UniqueCollection()
        self._materials = UniqueCollection()
        self._float_textures = UniqueCollection()
        self._color_textures = UniqueCollection()
        self._light_sources = []

        self._out_mesh_id = 0
        self._out_mesh_folder = out_mesh_folder

        self._color1_texture_id = self._create_texture_id({
            "type": "color",
            "value": [1.0, 1.0, 1.0]
        })
        self._float1_texture_id = self._create_texture_id({
            "type": "float",
            "value": 1.0
        })

        self._create_light_sources(pbrt_scene)
        self._create_scene_objects(pbrt_scene)
        self._create_scene_objects_instancing(pbrt_scene)


    def _create_light_sources(self, pbrt_scene):
        #print(pbrt_scene["light_sources"])
        for light_source in pbrt_scene["light_sources"]:
            if light_source.type == "infinite":
                if "samples" in light_source.arguments:
                    num_samples = light_source.arguments["samples"]["value"]
                else:
                    num_samples = 1

                if "L" in light_source.arguments:
                    L = light_source.arguments["L"]["value"]
                else:
                    L = [1.0, 1.0, 1.0]

                if "scale" in light_source.arguments:
                    L *= light_source.arguments["scale"]["value"]

                if "mapname" in light_source.arguments:
                    texture_id = self._create_texture_id(
                        light_source.arguments["mapname"])
                else:
                    texture_id = self._color1_texture_id
                self._light_sources.append({
                    "type": "infinite",
                    "texture": texture_id,
                    "scale": L,
                    "num_samples": num_samples,
                    "transform": light_source.transform
                })
            else:
                print(
                    f"WARNING: skipping light source of unsupported type {light_source.type}")

    def _create_texture_id(self, texture_info):
        tex_type = texture_info["type"]
        if tex_type == "float":
            # Inline texture property
            texture = Texture(type=tex_type, texture_class="constant", arguments={
                              "value": texture_info["value"]})
        elif tex_type == "spectrum" or tex_type == "color" or tex_type == "rgb":
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
            if "sigma" in material.arguments:
                sigma = self._create_texture_id(material.arguments["sigma"])
            else:
                sigma = self._create_texture_id(
                    {"type": "float", "value": 0.0})

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

    def _create_geometric_scene_object(self, shape):
        if shape.area_light is not None:
            print(f"TODO: Area light: {shape.area_light}")

        if shape.type == "plymesh":
            geometry_id = self._geometry.add_item({
                "type": "triangle",
                "filename": shape.arguments["filename"]["value"],
                "transform": shape.transform
            })
        elif shape.type == "trianglemesh":
            with open(shape.arguments["filename"], "rb") as f:
                filename = self._trianglemesh_to_obj(pickle.load(f))

            geometry_id = self._geometry.add_item({
                "type": "triangle",
                "filename": filename,
                "transform": shape.transform
            })
        else:
            print(f"Ignoring shape of unsupported type {shape.type}")
            return None

        material_id = self._create_material_id(shape.material)
        return {
            "instancing": False,
            "geometry_id": geometry_id,
            "material_id": material_id
        }

    def _create_scene_objects(self, pbrt_scene):
        for json_shape in pbrt_scene["non_instanced_shapes"]:
            scene_object = self._create_geometric_scene_object(json_shape)
            if scene_object is not None:
                self._scene_objects.add_item(scene_object)

    def _create_scene_objects_instancing(self, pbrt_scene):
        named_base_scene_objecst = {}
        for instance_template in pbrt_scene["instance_templates"].values():
            base_scene_objects = [self._create_geometric_scene_object(shape)
                                  for shape in instance_template.shapes]
            base_scene_objects = [
                so for so in base_scene_objects if so is not None]

            base_so_ids = [self._instance_base_scene_objects.add_item(
                so) for so in base_scene_objects]
            named_base_scene_objecst[instance_template.name] = base_so_ids

        num_instances = len(pbrt_scene["instances"])
        for i, instance in enumerate(pbrt_scene["instances"]):
            # if i % 1000 == 0:
            #    print(f"instance {i} / {num_instances-1}")
            for base_scene_object_id in named_base_scene_objecst[instance.template_name]:
                self._scene_objects.add_item({
                    "instancing": True,
                    "base_scene_object_id": base_scene_object_id,
                    "transform": instance.transform
                })

    def _trianglemesh_to_obj(self, geometry):
        mesh_id = self._out_mesh_id
        self._out_mesh_id += 1

        mesh_file = os.path.join(self._out_mesh_folder, f"mesh{mesh_id}.obj")
        with open(mesh_file, "w") as f:
            f.write("o PandoraMesh\n")

            positions = geometry["P"]["value"]
            for p0, p1, p2 in zip(*[positions[x::3] for x in (0, 1, 2)]):
                f.write(f"v {p0} {p1} {p2}\n")

            if "N" in geometry and "uv" in geometry:
                normals = geometry["N"]["value"]
                for n0, n1, n2 in zip(*[normals[x::3] for x in (0, 1, 2)]):
                    f.write(f"vn {n0} {n1} {n2}\n")

                uv_coords = geometry["uv"]["value"]
                for uv0, uv1 in zip(*[uv_coords[x::2] for x in (0, 1)]):
                    f.write(f"vt {uv0} {uv1}\n")

                indices = geometry["indices"]["value"]
                for i0, i1, i2 in zip(*[indices[x::3] for x in (0, 1, 2)]):
                    # OBJ starts counting at 1...
                    f.write(
                        f"f {i0+1}/{i0+1}/{i0+1} {i1+1}/{i1+1}/{i1+1} {i2+1}/{i2+1}/{i2+1}\n")
            else:
                for i0, i1, i2 in zip(*[indices[x::3] for x in (0, 1, 2)]):
                    # OBJ starts counting at 1...
                    f.write(f"f {i0+1} {i1+1} {i2+1}\n")

        return mesh_file

    def data(self):
        return {
            "lights": self._light_sources,
            "geometry": self._geometry.get_list(),
            "scene_objects": self._scene_objects.get_list(),
            "instance_base_scene_objects": self._instance_base_scene_objects.get_list(),
            "materials": self._materials.get_list(),
            "float_textures": self._float_textures.get_list(),
            "color_textures": self._color_textures.get_list()
        }
