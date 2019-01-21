from parsing.parser import Texture
from unique_collection import UniqueCollection
import numpy as np
import pickle
import os
import plyfile
from batched_obj_exporter import BatchedObjExporter
import sys
import tempfile
import pathlib
import shutil
import tempfile


def constant_texture(v):
    if isinstance(v, float):
        return Texture(
            "float",  # Type
            "constant",  # Class
            {  # Arguments
                "value": {
                    "type": "float",
                    "value": v
                }
            }
        )
    elif isinstance(v, list):
        return Texture(
            "color",  # Type
            "constant",  # Class
            {  # Arguments
                "value": {
                    "type": "color",
                    "value": np.array(v)
                }
            }
        )
    else:
        print(f"Trying to create constant texture for unknown value \"{v}\"")


def image_texture(mapname):
    return Texture(
        "color",  # Type
        "imagemap",  # Class
        {  # Arguments
            "filename": {
                "type": "string",
                "value": mapname
            }
        }
    )


def get_argument_with_default(arguments, name, default):
    if name in arguments:
        ret = arguments[name]["value"]
        return ret
    else:
        return default


def _replace_black_body(v):
    if isinstance(v, dict) and "blackbody_temperature_kelvin" in v:
        print(
            "WARNING: pandora does not handle blackbody emiters yet. Replacing by white...")
        return [1.0, 1.0, 1.0]
    else:
        return list(v)


def get_size(obj, seen=None):
    """Recursively finds size of objects"""
    size = sys.getsizeof(obj)
    if seen is None:
        seen = set()
    obj_id = id(obj)
    if obj_id in seen:
        return 0
    # Important mark as seen *before* entering recursion to gracefully handle
    # self-referential objects
    seen.add(obj_id)
    if isinstance(obj, dict):
        size += sum([get_size(v, seen) for v in obj.values()])
        size += sum([get_size(k, seen) for k in obj.keys()])
    elif hasattr(obj, '__dict__'):
        size += get_size(obj.__dict__, seen)
    elif hasattr(obj, '__iter__') and not isinstance(obj, (str, bytes, bytearray)):
        size += sum([get_size(i, seen) for i in obj])
    return size


class SceneParser:
    def __init__(self, pbrt_scene, out_folder):
        self._out_folder = out_folder
        self._tmp_list_folder = os.path.join(out_folder, "tmp_lists")
        self._out_texture_folder = os.path.join(out_folder, "textures")
        if not os.path.exists(self._out_texture_folder):
            os.makedirs(self._out_texture_folder)

        self._transforms = UniqueCollection(
            os.path.join(self._tmp_list_folder, "transforms"))
        self._geometry = UniqueCollection(
            os.path.join(self._tmp_list_folder, "geometry"))
        self._scene_objects = UniqueCollection(
            os.path.join(self._tmp_list_folder, "scene_objects"))
        self._instance_base_scene_objects = UniqueCollection(
            os.path.join(self._tmp_list_folder, "instance_base_scene_objects"))
        self._materials = UniqueCollection(
            os.path.join(self._tmp_list_folder, "materials"))
        self._float_textures = UniqueCollection(
            os.path.join(self._tmp_list_folder, "float_textures"))
        self._color_textures = UniqueCollection(
            os.path.join(self._tmp_list_folder, "color_textures"))
        self._light_sources = []

        out_mesh_folder = os.path.join(out_folder, "pandora_meshes")
        self._out_mesh_exporter = BatchedObjExporter(out_mesh_folder)
        self._out_ply_mesh_folder = os.path.join(out_mesh_folder, "ply")
        if os.path.exists(self._out_ply_mesh_folder):
            shutil.rmtree(self._out_ply_mesh_folder)
        os.makedirs(self._out_ply_mesh_folder)

        self._pbrt_named_textures = pbrt_scene["textures"]

        self._create_light_sources(pbrt_scene)
        self._create_scene_objects(pbrt_scene)
        self._create_scene_objects_instancing(pbrt_scene)

    def _dump_mem_info(self):
        print("=== Memory size info ===")
        print("self._transforms:          ", get_size(self._transforms))
        print("self._geometry:          ", get_size(self._geometry))
        print("self._scene_objects:     ", get_size(self._scene_objects))
        print("self._instance_base_scene_objects: ",
              get_size(self._instance_base_scene_objects))
        print("self._materials:         ", get_size(self._materials))
        print("self._float_textures:    ", get_size(self._float_textures))
        print("self._color_textures:    ", get_size(self._color_textures))
        print("self._light_sources:    ", get_size(self._light_sources))

    def _create_transform(self, transform):
        return self._transforms.add_item(transform)

    def _create_light_sources(self, pbrt_scene):
        for light_source in pbrt_scene["light_sources"]:
            scale = [1.0, 1.0, 1.0]
            if "scale" in light_source.arguments:
                scale = light_source.arguments["scale"]["value"]

            if light_source.type == "infinite":
                if "samples" in light_source.arguments:
                    num_samples = light_source.arguments["samples"]["value"]
                else:
                    num_samples = 1

                if "L" in light_source.arguments:
                    L = _replace_black_body(
                        light_source.arguments["L"]["value"])
                else:
                    L = [1.0, 1.0, 1.0]
                L = list(np.array(L) * np.array(scale))

                if "mapname" in light_source.arguments:
                    texture_id = self._create_texture_id(
                        image_texture(light_source.arguments["mapname"]["value"]))
                else:
                    texture_id = self._create_texture_id(constant_texture(1.0))
                self._light_sources.append({
                    "type": "infinite",
                    "texture": texture_id,
                    "scale": L,
                    "num_samples": num_samples,
                    "transform": self._create_transform(light_source.transform)
                })
            elif light_source.type == "distant":
                if "from" in light_source.arguments and "to" in light_source.arguments:
                    direction = light_source.arguments["to"]["value"] - \
                        light_source.arguments["from"]["value"]
                else:
                    direction = [0, 0, 1]

                if "L" in light_source.arguments:
                    L = _replace_black_body(
                        light_source.arguments["L"]["value"])
                else:
                    L = [1, 1, 1]
                L = list(np.array(L) * np.array(scale))

                self._light_sources.append({
                    "type": "distant",
                    "L": L,
                    "direction": list(direction),
                    "transform": self._create_transform(light_source.transform)
                })
            else:
                print(
                    f"WARNING: skipping light source of unsupported type {light_source.type}")

    def _create_texture_id(self, texture):
        # Unpack arguments
        arguments = {}
        for key, value in texture.arguments.items():
            v = value["value"]

            if key == "filename":
                # Copy to the destination folder so that we can use relative paths
                suffix = pathlib.PurePath(v).suffix
                with tempfile.NamedTemporaryFile("wb+", dir=self._out_texture_folder, suffix=suffix, delete=False) as out_file:
                    with open(v, "rb") as in_file:
                        out_file.write(in_file.read())
                    v = os.path.relpath(out_file.name, self._out_folder)
                    #print(f"{out_file.name} is {v} relative to {self._out_folder}")

            arguments[key] = v

        texture_type = None
        if texture.type in ["spectrum", "rgb", "color"]:
            texture_type = "color"
        elif texture.type == "float":
            texture_type = "float"
        else:
            print(f"Unknown texture type \"{texture.type}\"")

        texture_class = texture.texture_class
        """
        if "filename" in arguments:
            if texture_type == "float":
                arguments = { "value": 0.5 }
            else:
                arguments = { "value": np.array([0.5, 0.5, 0.5]) }
            texture_class = "constant"
        """

        texture_dict = {
            "type": texture_type,
            "class": texture_class,
            "arguments": arguments
        }

        if texture_dict["type"] == "float":
            return self._float_textures.add_item(texture_dict)
        else:
            return self._color_textures.add_item(texture_dict)

    def _float_tex_argument(self, argument):
        t = argument["type"]
        v = argument["value"]
        if t == "texture":
            return self._create_texture_id(self._pbrt_named_textures[v])
        else:
            assert(t == "float")
            return self._create_texture_id(constant_texture(v))

    def _color_tex_argument(self, argument):
        t = argument["type"]
        v = argument["value"]
        if t == "texture":
            return self._create_texture_id(self._pbrt_named_textures[v])
        else:
            assert(t == "spectrum" or t == "color" or t == "rgb")
            return self._create_texture_id(constant_texture(list(v)))

    def _create_material_id(self, material):
        if material.type == "matte":
            if "Kd" in material.arguments:
                kd = self._color_tex_argument(material.arguments["Kd"])
            else:
                kd = self._create_texture_id(constant_texture([0.5, 0.5, 0.5]))

            if "sigma" in material.arguments:
                sigma = self._float_tex_argument(material.arguments["sigma"])
            else:
                sigma = self._create_texture_id(constant_texture(0.0))

            # Leave out texture support for now?
            return self._materials.add_item({
                "type": "matte",
                "arguments": {
                    "kd": kd,
                    "sigma": sigma
                }
            })
        else:
            # print(
            #    f"Material type {material.type} is not supported yet. Replacing by matte white placeholder.")

            kd = self._create_texture_id(constant_texture([1.0, 1.0, 1.0]))
            sigma = self._create_texture_id(constant_texture(0.5))

            return self._materials.add_item({
                "type": "matte",
                "arguments": {
                    "kd": kd,
                    "sigma": sigma
                }
            })

    def _create_geometric_scene_object(self, shape):
        if shape.type == "plymesh":
            handle, filename = tempfile.mkstemp(
                suffix=".ply", dir=self._out_ply_mesh_folder)
            os.close(handle)

            shutil.copyfile(shape.arguments["filename"]["value"], filename)

            filename = os.path.relpath(filename, start=self._out_folder)
            geometry_id = self._geometry.add_item({
                "type": "triangle",
                "filename": filename,
                "transform": self._create_transform(shape.transform)
            })
        elif shape.type == "trianglemesh":
            with open(shape.arguments["filename"], "rb") as f:
                f.seek(shape.arguments["start_byte"])
                string = f.read(shape.arguments["num_bytes"])
                triangle_mesh_data = pickle.loads(string)
                filename, start_byte, size_bytes = self._export_triangle_mesh(
                    triangle_mesh_data, shape.transform)

            filename = os.path.relpath(filename, start=self._out_folder)
            geometry_id = self._geometry.add_item({
                "type": "triangle",
                "filename": filename,
                "start_byte": start_byte,
                "size_bytes": size_bytes
                #"transform": self._create_transform(shape.transform)
            })
        else:
            # print(f"Ignoring shape of unsupported type {shape.type}")
            return None

        material_id = self._create_material_id(shape.material)

        if shape.area_light is not None:
            L =  _replace_black_body(shape.area_light.arguments["L"]["value"])
            if "scale" in shape.area_light.arguments:
                L = list(np.array(L) * np.array(shape.area_light.arguments["scale"]["value"]))
            area_light = {
                # get_argument_with_default(shape.area_light.arguments, "L", [1,1,1]),
                "L": L,
                "num_samples": get_argument_with_default(shape.area_light.arguments, "nsamples", 1),
                "two_sided": get_argument_with_default(shape.area_light.arguments, "twosided", False)
            }
            return {
                "instancing": False,
                "geometry_id": geometry_id,
                "material_id": material_id,
                "area_light": area_light,
                # "bounds": shape_bounds
            }
        else:
            return {
                "instancing": False,
                "geometry_id": geometry_id,
                "material_id": material_id,
                # "bounds": shape_bounds
            }

    def _create_scene_objects(self, pbrt_scene):
        print("Unique scene objects")

        for json_shape in pbrt_scene["non_instanced_shapes"]:
            scene_object = self._create_geometric_scene_object(json_shape)
            if scene_object is not None:
                self._scene_objects.add_item(scene_object)

    def _create_scene_objects_instancing(self, pbrt_scene):
        print("Instanced scene objects")

        named_base_scene_objects = {}
        for instance_template in pbrt_scene["instance_templates"].values():
            base_scene_objects = [self._create_geometric_scene_object(shape)
                                  for shape in instance_template.shapes]
            base_scene_objects = [
                so for so in base_scene_objects if so is not None]

            base_so_ids = [self._instance_base_scene_objects.add_item(
                so) for so in base_scene_objects]
            named_base_scene_objects[instance_template.name] = base_so_ids

        for instance in pbrt_scene["instances"]:
            for base_scene_object_id in named_base_scene_objects[instance.template_name]:
                self._scene_objects.add_item({
                    "instancing": True,
                    "base_scene_object_id": base_scene_object_id,
                    "transform": self._create_transform(instance.transform)
                })

    def _export_triangle_mesh(self, geometry, transform):
        triangles = geometry["indices"]["value"]
        positions = geometry["P"]["value"]

        if "normals" in geometry:
            normals = geometry["normals"]["value"]
        else:
            normals = np.empty((0))

        if "tangents" in geometry:
            tangents = geometry["tangents"]["value"]
        else:
            tangents = np.empty((0))

        if "uv" in geometry:
            uv_coords = geometry["uv"]["value"]
        else:
            uv_coords = np.empty((0))

        return self._out_mesh_exporter.add_triangle_mesh(triangles, positions, normals, tangents, uv_coords, transform)

    def data(self):
        self._transforms.finish()
        self._geometry.finish()
        self._scene_objects.finish()
        self._instance_base_scene_objects.finish()
        self._materials.finish()
        self._float_textures.finish()
        self._color_textures.finish()

        return {
            "transforms": self._transforms.to_list(),
            "lights": self._light_sources,
            "geometry": self._geometry.to_list(),
            "scene_objects": self._scene_objects.to_list(),
            "instance_base_scene_objects": self._instance_base_scene_objects.to_list(),
            "materials": self._materials.to_list(),
            "float_textures": self._float_textures.to_list(),
            "color_textures": self._color_textures.to_list()
        }
