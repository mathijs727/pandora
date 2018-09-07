from parsing.parser import Texture
from unique_collection import UniqueCollection
import numpy as np
import pickle
import os


def constant_texture(v):
    if isinstance(v, float):
        return Texture(
            "float",# Type
            "constant",# Class
            { # Arguments
                "value": {
                    "type": "float",
                    "value": 0.5
                }
            }
        )
    elif isinstance(v, list):
        return Texture(
            "color",# Type
            "constant",# Class
            { # Arguments
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
        "color",# Type
        "imagemap",# Class
        { # Arguments
            "filename": {
                "type": "string",
                "value": mapname
            }
        }
    )


def get_argument_with_default(arguments, name, default):
    if name in arguments:
        ret = arguments[name]["value"]
        if isinstance(ret, np.ndarray):
            return list(ret)
        else:
            return ret
    else:
        return default


def _replace_black_body(v):
    if isinstance(v, dict) and "blackbody_temperature_kelvin" in v:
        print("WARNING: pandora does not handle blackbody emiters yet. Replacing by white...")
        return [1.0, 1.0, 1.0]
    else:
        return v

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

        self._pbrt_named_textures = pbrt_scene["textures"]

        self._create_light_sources(pbrt_scene)
        self._create_scene_objects(pbrt_scene)
        self._create_scene_objects_instancing(pbrt_scene)


    def _create_light_sources(self, pbrt_scene):
        # print(pbrt_scene["light_sources"])
        for light_source in pbrt_scene["light_sources"]:
            if light_source.type == "infinite":
                if "samples" in light_source.arguments:
                    num_samples = light_source.arguments["samples"]["value"]
                else:
                    num_samples = 1

                if "L" in light_source.arguments:
                    L = _replace_black_body(light_source.arguments["L"]["value"])
                else:
                    L = [1.0, 1.0, 1.0]

                if "scale" in light_source.arguments:
                    L *= light_source.arguments["scale"]["value"]

                if "mapname" in light_source.arguments:
                    texture_id = self._create_texture_id(image_texture(light_source.arguments["mapname"]["value"]))
                else:
                    texture_id = self._create_texture_id(constant_texture(1.0))
                self._light_sources.append({
                    "type": "infinite",
                    "texture": texture_id,
                    "scale": L,
                    "num_samples": num_samples,
                    "transform": light_source.transform
                })
            elif light_source.type == "distant":
                if "from" in light_source.arguments and "to" in light_source.arguments:
                    direction = light_source.arguments["to"]["value"] -light_source.arguments["from"]["value"]
                else:
                    direction = [0, 0, 1]
                
                if "L" in light_source.arguments:
                    L = _replace_black_body(light_source.arguments["L"]["value"])
                else:
                    L = [1,1,1]

                self._light_sources.append({
                    "type": "distant",
                    "L": L,
                    "direction": list(direction),
                    "transform": light_source.transform
                })
            else:
                print(
                    f"WARNING: skipping light source of unsupported type {light_source.type}")

    def _create_texture_id(self, texture):
        # Unpack arguments
        arguments = {}
        for key, value in texture.arguments.items():
            v = value["value"]
            if isinstance(v, np.ndarray):
                v = list(v)
            arguments[key] = v
        
        texture_type = None
        if texture.type in ["spectrum", "rgb", "color"]:
            texture_type = "color"
        elif texture.type == "float":
            texture_type = "float"
        else:
            print(f"Unknown texture type \"{texture.type}\"")

        texture_dict = {
            "type": texture_type if texture_type != "spectrum" else "color",
            "class": texture.texture_class,
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
                kd = self._create_texture_id(constant_texture([0, 0, 0]))

            if "sigma" in material.arguments:
                sigma = self._float_tex_argument(material.arguments["sigma"])
            else:
                sigma = self._create_texture_id(constant_texture(0.5))

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
            geometry_id=self._geometry.add_item({
                "type": "triangle",
                "filename": shape.arguments["filename"]["value"],
                "transform": shape.transform
            })
        elif shape.type == "trianglemesh":
            with open(shape.arguments["filename"], "rb") as f:
                filename=self._trianglemesh_to_obj(pickle.load(f))

            geometry_id=self._geometry.add_item({
                "type": "triangle",
                "filename": filename,
                "transform": shape.transform
            })
        else:
            print(f"Ignoring shape of unsupported type {shape.type}")
            return None

        material_id=self._create_material_id(shape.material)

        if shape.area_light is not None:
            area_light = {
                "L": _replace_black_body(shape.area_light.arguments["L"]["value"]),#get_argument_with_default(shape.area_light.arguments, "L", [1,1,1]),
                "num_samples": get_argument_with_default(shape.area_light.arguments, "nsamples", 1),
                "two_sided": get_argument_with_default(shape.area_light.arguments, "twosided", False)
            }
            return {
                "instancing": False,
                "geometry_id": geometry_id,
                "material_id": material_id,
                "area_light": area_light
            }
        else:
            return {
                "instancing": False,
                "geometry_id": geometry_id,
                "material_id": material_id
            }

    def _create_scene_objects(self, pbrt_scene):
        for json_shape in pbrt_scene["non_instanced_shapes"]:
            scene_object=self._create_geometric_scene_object(json_shape)
            if scene_object is not None:
                self._scene_objects.add_item(scene_object)

    def _create_scene_objects_instancing(self, pbrt_scene):
        named_base_scene_objecst={}
        for instance_template in pbrt_scene["instance_templates"].values():
            base_scene_objects=[self._create_geometric_scene_object(shape)
                                  for shape in instance_template.shapes]
            base_scene_objects=[
                so for so in base_scene_objects if so is not None]

            base_so_ids=[self._instance_base_scene_objects.add_item(
                so) for so in base_scene_objects]
            named_base_scene_objecst[instance_template.name]=base_so_ids

        num_instances=len(pbrt_scene["instances"])
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
        mesh_id=self._out_mesh_id
        self._out_mesh_id += 1

        mesh_file=os.path.join(self._out_mesh_folder, f"mesh{mesh_id}.obj")
        with open(mesh_file, "w") as f:
            f.write("o PandoraMesh\n")

            positions=geometry["P"]["value"]
            for p0, p1, p2 in zip(*[positions[x::3] for x in (0, 1, 2)]):
                f.write(f"v {p0} {p1} {p2}\n")

            if "N" in geometry and "uv" in geometry:
                normals=geometry["N"]["value"]
                for n0, n1, n2 in zip(*[normals[x::3] for x in (0, 1, 2)]):
                    f.write(f"vn {n0} {n1} {n2}\n")

                uv_coords=geometry["uv"]["value"]
                for uv0, uv1 in zip(*[uv_coords[x::2] for x in (0, 1)]):
                    f.write(f"vt {uv0} {uv1}\n")

                indices=geometry["indices"]["value"]
                for i0, i1, i2 in zip(*[indices[x::3] for x in (0, 1, 2)]):
                    # OBJ starts counting at 1...
                    f.write(
                        f"f {i0+1}/{i0+1}/{i0+1} {i1+1}/{i1+1}/{i1+1} {i2+1}/{i2+1}/{i2+1}\n")
            else:
                indices=geometry["indices"]["value"]
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
