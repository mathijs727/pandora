from deepmerge import merge_or_raise
import numpy as np
import os
import sys
from collections import namedtuple
from enum import Enum
import pickle
from parsing.matrix import translate, scale, rotate, lookat
from parsing.lexer import create_lexer, tokens
from parsing.mesh_batch import MeshBatcher
from parsing.file_backed_list import FileBackedList
import parsing.lexer
import ply.yacc as yacc

import itertools
import collections

# Get pandora_py from parent path
# https://stackoverflow.com/questions/16780014/import-file-from-parent-directory
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import pandora_py


class ParsingState(Enum):
    CONFIG = 1
    SCENE = 2


parsing_state = ParsingState.CONFIG
base_path = ""
current_file = ""
mesh_batcher = None


def p_statement_main(p):
    "statement_main : statements_config world_begin statements_scene WORLD_END"
    scene_data = {
        "textures": named_textures,
        "light_sources": light_sources,
        "instance_templates": instance_templates,
        "instances": instances,
        "non_instanced_shapes": non_instanced_shapes
    }

    p[0] = {"config": p[1], "scene": scene_data}


def p_statements_config(p):
    """statements_config : statements_config statement_config
                         | """
    if len(p) == 3:
        # Combine the two dictionaries
        if p[2] is None:
            p[0] = p[1]
        else:
            p[0] = merge_or_raise.merge(p[1], p[2])
    else:
        p[0] = {}


def p_statements_scene(p):
    """statements_scene : statements_scene statement_scene
                         | """


def p_statement_include(p):
    "statement_include : INCLUDE STRING"
    global parsing_state, base_path, current_file

    # Store state
    current_file_bak = parsing.lexer.current_file

    # Set new state
    include_file = os.path.join(base_path, p[2])
    parsing.lexer.current_file = include_file
    current_file = include_file

    # print_mem_info()

    print(f"Processing include file {include_file}")
    with open(include_file, "r") as f:
        lexer = create_lexer()
        if parsing_state == ParsingState.CONFIG:
            parser = yacc.yacc(start="statements_config")
        else:
            parser = yacc.yacc(start="statements_scene")
        return parser.parse(f.read(), lexer=lexer)

    # Restore state
    parsing.lexer.current_file = current_file_bak
    current_file = current_file_bak

    return None


def p_statements_scene_include(p):
    """statement_scene : statement_include"""
    p[0] = p[1]


def p_statements_config_include(p):
    """statement_config : statement_include"""
    p[0] = p[1]


Camera = namedtuple(
    "Camera", ["type", "arguments", "world_to_camera_transform"])
LightSource = namedtuple("LightSource", ["type", "arguments", "transform"])
AreaLightSource = namedtuple("AreaLightSource", ["type", "arguments"])
Shape = namedtuple("Shape", ["type", "arguments",
                             "transform", "flip_normals", "material", "area_light"])
InstanceTemplate = namedtuple(
    "InstanceTemplate", ["name", "shapes", "transform"])
Instance = namedtuple(
    "Instance", ["template_name", "transform"])
Material = namedtuple("Material", ["type", "arguments"])
Texture = namedtuple("Texture", ["type", "texture_class", "arguments"])

SampledSpectrum = namedtuple("SampledSpectrum", ["values", "wavelengths_nm"])
SampledSpectrumFile = namedtuple("SampledSpectrumFile", ["filename"])
#TextureRef = namedtuple("TextureRef", ["name"])
BlackBody = namedtuple("BlackBody", ["temperature_kelvin", "scale_factor"])

named_materials = {}
named_textures = {}
light_sources = []
instance_templates = {}
instances = None  # FileBackedList
non_instanced_shapes = None  # FileBackedList


default_material = Material(
    type="matte",
    arguments={
        "Kd": {
            "type": "color",
            "value": np.array([0.5, 0.5, 0.5])
        },
        "sigma": {
            "type": "float",
            "value": 0.0
        }})
graphics_state_stack = []
graphics_state = {"area_light": None,
                  "flip_normals": False, "material": default_material}
transform_stack = []
named_transforms = {}
cur_transform = np.identity(4)
current_instance = None


# https://code.activestate.com/recipes/577504/
def total_size(o, handlers={}, verbose=False):
    """ Returns the approximate memory footprint an object and all of its contents.

    Automatically finds the contents of the following builtin containers and
    their subclasses:  tuple, list, deque, dict, set and frozenset.
    To search other containers, add handlers to iterate over their contents:

        handlers = {SomeContainerClass: iter,
                    OtherContainerClass: OtherContainerClass.get_elements}

    """
    def dict_handler(d): return itertools.chain.from_iterable(d.items())
    all_handlers = {tuple: iter,
                    list: iter,
                    collections.deque: iter,
                    dict: dict_handler,
                    set: iter,
                    frozenset: iter,
                    }
    all_handlers.update(handlers)     # user handlers take precedence
    seen = set()                      # track which object id's have already been seen
    # estimate sizeof object without __sizeof__
    default_size = sys.getsizeof(0)

    def sizeof(o):
        if id(o) in seen:       # do not double count the same object
            return 0
        seen.add(id(o))
        s = sys.getsizeof(o, default_size)

        if verbose:
            print(s, type(o), repr(o), file=sys.stderr)

        for typ, handler in all_handlers.items():
            if isinstance(o, typ):
                s += sum(map(sizeof, handler(o)))
                break
        return s

    return sizeof(o)


def print_mem_info():
    print("++++ MEMORY USAGE ++++")
    print("named_materials: ", total_size(named_materials) / 1000, "KB")
    print("named_textures: ", total_size(named_textures) / 1000, "KB")
    print("light_sources: ", total_size(light_sources) / 1000, "KB")
    print("instance_templates: ", total_size(
        instance_templates) / 1000, "KB")
    print("instances: ", total_size(instances) / 1000, "KB")
    print("non_instanced_shapes: ", total_size(
        non_instanced_shapes) / 1000, "KB")
    print("graphics_state_stack: ", total_size(
        graphics_state_stack) / 1000, "KB")
    print("graphics_state: ", total_size(graphics_state) / 1000, "KB")
    print("transform_stack: ", total_size(transform_stack) / 1000, "KB")
    print("named_transforms: ", total_size(named_transforms) / 1000, "KB")
    print("cur_transform: ", total_size(cur_transform) / 1000, "KB")
    print("current_instance: ", total_size(current_instance) / 1000, "KB")


def p_error(p):
    print(
        f"Syntax error: unexpected token \"{p.value}\" in file \"{current_file}\" at line {p.lineno}")


def p_basic_data_type(p):
    """basic_data_type : STRING
                       | NUMBER"""
    p[0] = p[1]
    lineno = p.lineno(n=1)


def p_list(p):
    "list : LIST"
    text = p[1][1:-1]
    if '"' in text:
        import re
        p[0] = [s[1:-1] for s in re.findall('"[^"]*"', text)]
    elif '.' in text:
        #result = np.fromstring(text, dtype=float, sep=' ')
        # The Python string length in C++ is a 32-bit int
        assert(len(text) < 2147483647)
        # Neither ujson nor rapidjson support float32 so convert to a double
        result = pandora_py.string_to_numpy_double(text)
        p[0] = result
    else:
        #result = np.fromstring(text, dtype=int, sep=' ')
        # The Python string length in C++ is a 32-bit int
        assert(len(text) < 2147483647)
        result = pandora_py.string_to_numpy_int(text)
        p[0] = result


def p_argument(p):
    """argument : STRING basic_data_type
                | STRING list"""
    global base_path, named_textures
    arg_type, arg_name = p[1].split()
    data = p[2]

    # Unfold lists of single length
    if (isinstance(data, np.ndarray) or isinstance(data, list)) and len(data) == 1:
        data = data[0]

    float_types = ["float", "rgb", "xyz", "color", "spectrum", "point", "point2", "point3", "normal", "normal2", "normal3",
                   "vector", "vector2", "vector3"]

    if isinstance(data, np.ndarray):
        if arg_type == "integer":
            p[0] = {arg_name: {"type": arg_type, "value": data.astype(int)}}
        elif arg_type in float_types:
            p[0] = {arg_name: {"type": arg_type,
                               "value": data}}
        elif arg_type == "blackbody":
            assert(len(data) == 2)
            p[0] = {arg_name: {
                "type": arg_type,
                "value": {
                    "blackbody_temperature_kelvin": float(data[0]),
                    "blackbody_scale_factor": float(data[1])
                }
            }}
        else:
            print(data)
            raise RuntimeError(f"Unknown argument type {arg_type}")
    else:
        if arg_type == "integer":
            p[0] = {arg_name: {"type": arg_type, "value": int(data)}}
        elif arg_type == "float":
            p[0] = {arg_name: {"type": arg_type, "value": float(data)}}
        elif arg_type == "bool":
            p[0] = {arg_name: {"type": arg_type,
                               "value": True if data == "true" else False}}
        elif arg_type == "string":
            p[0] = {arg_name: {"type": arg_type, "value": str(data)}}
        elif arg_type == "texture":
            p[0] = {arg_name: {"type": arg_type, "value": data}}
        elif arg_type == "spectrum":
            p[0] = {arg_name: {"type": arg_type, "value": SampledSpectrumFile(
                os.path.join(base_path, data))}}
        else:
            print(data)
            raise RuntimeError(f"Unknown argument type {arg_type}")


def p_arguments(p):
    """arguments : arguments argument
                 | """
    if len(p) == 3:
        # Merge dictionaries
        p[1].update(p[2])
        p[0] = p[1]
    else:
        p[0] = {}


def p_statement_world_begin(p):
    "world_begin : WORLD_BEGIN"
    global named_transforms, cur_transform, parsing_state
    named_transforms["world"] = cur_transform
    cur_transform = np.identity(4)
    parsing_state = ParsingState.SCENE


def p_statement_attribute_begin(p):
    "statement_scene : ATTRIBUTE_BEGIN"
    global graphics_state, graphics_state_stack, cur_transform, transform_stack
    graphics_state_stack.append(graphics_state.copy())
    transform_stack.append(cur_transform.copy())


def p_statement_attribute_end(p):
    "statement_scene : ATTRIBUTE_END"
    global graphics_state, graphics_state_stack, cur_transform, transform_stack
    graphics_state = graphics_state_stack.pop()
    cur_transform = transform_stack.pop()


def p_statement_transform_begin(p):
    "statement_scene : TRANSFORM_BEGIN"
    global cur_transform, transform_stack
    transform_stack.append(cur_transform.copy())


def p_statement_transform_end(p):
    "statement_scene : TRANSFORM_END"
    global cur_transform, transform_stack
    cur_transform = transform_stack.pop()


def p_statement_scene_transform(p):
    "statement_scene : statement_transform"
    p[0] = p[1]


def p_statement_transform_identity(p):
    "statement_transform : IDENTITY"
    global cur_transform
    cur_transform = np.identity(4)


def p_statement_transform_translate(p):
    "statement_transform : TRANSLATE NUMBER NUMBER NUMBER"
    global cur_transform
    xyz = np.array([p[2], p[3], p[4]])
    cur_transform = np.matmul(cur_transform, translate(xyz))


def p_statement_transform_scale(p):
    "statement_transform : SCALE NUMBER NUMBER NUMBER"
    global cur_transform
    xyz = np.array([p[2], p[3], p[4]])
    cur_transform = np.matmul(cur_transform, scale(xyz))


def p_statement_transform_rotate(p):
    "statement_transform : ROTATE NUMBER NUMBER NUMBER NUMBER"
    global cur_transform
    angle = p[2]
    xyz = np.array([p[3], p[4], p[5]])
    cur_transform = np.matmul(cur_transform, rotate(angle, xyz))


def p_statement_transform_lookat(p):
    "statement_transform : LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER"
    global cur_transform
    eye = np.array([p[2], p[3], p[4]])
    target = np.array([p[5], p[6], p[7]])
    up = np.array([p[8], p[9], p[10]])
    cur_transform = np.matmul(cur_transform, lookat(eye, target, up))


def p_statement_transform_coordinate_system(p):
    "statement_transform : COORDINATE_SYSTEM STRING"
    global named_transforms, cur_transform
    named_transforms[p[2]] = cur_transform


def p_statement_transform_coord_sys_transform(p):
    "statement_transform : COORDINATE_SYSTEM_TRANSFORM STRING"
    global named_transforms, cur_transform
    cur_transform = named_transforms[p[2]]


def p_statement_transform(p):
    "statement_transform : TRANSFORM list"
    #"statement_transform : TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET"
    global cur_transform
    matrix = np.transpose(np.array(p[2]).reshape((4, 4)))
    cur_transform = np.matmul(cur_transform, matrix)


def p_statement_transform_concat(p):
    "statement_transform : CONCAT_TRANSFORM list"
    #"statement_transform : CONCAT_TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET"
    global cur_transform
    matrix = np.transpose(np.array(p[2]).reshape((4, 4)))
    cur_transform = matrix


def p_statement_material(p):
    "statement_scene : MATERIAL STRING arguments"
    global graphics_state
    graphics_state["material"] = Material(type=p[2], arguments=p[3])


def p_statement_named_material(p):
    "statement_scene : NAMED_MATERIAL STRING"
    global graphics_state, named_materials
    material_name = p[2]
    if material_name in named_materials:
        graphics_state["material"] = named_materials[material_name]
    else:
        print(
            f"WARNING: named material {material_name} was not declared! Ignoring...")


def p_statement_make_named_material(p):
    "statement_scene : MAKE_NAMED_MATERIAL STRING arguments"
    global named_materials
    arguments = p[3]
    material_type = arguments["type"]["value"]
    del arguments["type"]
    named_materials[p[2]] = Material(type=material_type, arguments=arguments)


def p_statement_texture(p):
    "statement_scene : TEXTURE STRING STRING STRING arguments"
    global named_textures
    arguments = p[5]
    if "filename" in arguments:
        if isinstance(arguments["filename"], list):
            arguments["filename"] = [os.path.join(
                base_path, f) for f in arguments["filename"]]
        else:
            arguments["filename"]["value"] = os.path.join(
                base_path, arguments["filename"]["value"])
    named_textures[p[2]] = Texture(
        type=p[3], texture_class=p[4], arguments=arguments)


def p_statement_light(p):
    "statement_scene : LIGHT_SOURCE STRING arguments"
    global light_sources, cur_transform
    light_type = p[2]
    arguments = p[3]
    if "mapname" in arguments:
        arguments["mapname"]["value"] = os.path.join(
            base_path, arguments["mapname"]["value"])
    light_sources.append(LightSource(
        light_type, arguments, cur_transform.tolist()))


def p_statement_area_light(p):
    "statement_scene : AREA_LIGHT_SOURCE STRING arguments"
    global graphics_state
    light_type = p[2]
    arguments = p[3]
    if "mapname" in arguments:
        arguments["mapname"]["value"] = os.path.join(
            base_path, arguments["mapname"]["value"])
    graphics_state["area_light"] = AreaLightSource(light_type, arguments)


def p_statement_object_begin(p):
    "statement_scene : OBJECT_BEGIN STRING"
    global current_instance
    current_instance = InstanceTemplate(p[2], [], cur_transform.tolist())


def p_statement_object_end(p):
    "statement_scene : OBJECT_END"
    global current_instance, instance_templates
    instance_templates[current_instance.name] = current_instance
    current_instance = None


def p_statement_object_instance(p):
    "statement_scene : OBJECT_INSTANCE STRING"
    global instances
    instances.append(
        Instance(template_name=p[2], transform=cur_transform.tolist()))


def p_statement_shape(p):
    "statement_scene : SHAPE STRING arguments"
    global non_instanced_shapes, current_instance, graphics_state, cur_transform, base_path
    shape_type = p[2]
    arguments = p[3]

    if shape_type == "plymesh":
        arguments["filename"]["value"] = os.path.join(
            base_path, arguments["filename"]["value"])
    else:
        global mesh_batcher
        filename, start_byte, num_bytes = mesh_batcher.add_mesh(arguments)
        arguments = {"filename": filename,
                     "start_byte": start_byte, "num_bytes": num_bytes}

    shape = Shape(shape_type, arguments, cur_transform.tolist(),
                  graphics_state["flip_normals"], graphics_state["material"], graphics_state["area_light"])
    if current_instance is None:
        non_instanced_shapes.append(shape)
    else:
        current_instance.shapes.append(shape)


def p_statement_reverse_orientation(p):
    "statement_scene : REVERSE_ORIENTATION"
    global graphics_state
    graphics_state["flip_normals"] = True


def p_statement_config_transform(p):
    "statement_config : statement_transform"
    p[0] = p[1]


def p_statement_camera(p):
    "statement_config : CAMERA STRING arguments"
    global cur_transform
    camera_type = p[2]
    arguments = p[3]
    p[0] = {
        "camera": Camera(type=camera_type, arguments=arguments, world_to_camera_transform=cur_transform.tolist())
    }


def p_statement_sampler(p):
    "statement_config : SAMPLER STRING arguments"
    sampler_type = p[2]
    arguments = p[3]
    # A PRBTv3 scene file may contain multiple samplers (ie the Crown scene)
    p[0] = {"samplers": [{"type": sampler_type, "arguments": arguments}]}


def p_statement_film(p):
    "statement_config : FILM STRING arguments"
    film_type = p[2]
    arguments = p[3]
    p[0] = {"films": [{"type": film_type, "arguments": arguments}]}


def p_statement_filter(p):
    "statement_config : FILTER STRING arguments"
    filter_type = p[2]
    arguments = p[3]
    p[0] = {"filters": [{"type": filter_type, "arguments": arguments}]}


def p_statement_integrator(p):
    "statement_config : INTEGRATOR STRING arguments"
    integrator_type = p[2]
    arguments = p[3]
    p[0] = {
        "integrator": {"type": integrator_type, "arguments": arguments}
    }


def p_statement_accelerator(p):
    "statement_config : ACCELERATOR STRING arguments"
    accelerator_type = p[2]
    arguments = p[3]
    p[0] = {
        "accelerator": {"type": accelerator_type, "arguments": arguments}
    }


def parse_file(file_path, int_mesh_folder):
    lexer = create_lexer()
    parser = yacc.yacc()

    import parsing.lexer
    parsing.lexer.current_file = file_path

    with open(file_path, "r") as f:
        string = f.read()

    global base_path, mesh_batcher, current_file
    if not os.path.exists(int_mesh_folder):
        os.makedirs(int_mesh_folder)
    mesh_batcher = MeshBatcher(int_mesh_folder)

    global non_instanced_shapes, instances
    shapes_folder = os.path.join(int_mesh_folder, "shapes")
    instances_folder = os.path.join(int_mesh_folder, "instances")
    non_instanced_shapes = FileBackedList(shapes_folder)
    instances = FileBackedList(instances_folder)

    current_file = os.path.abspath(file_path)
    parsing.lexer.current_file = current_file
    base_path = os.path.dirname(current_file)

    print("Base path: ", base_path)

    print("Parsing...")
    ret = parser.parse(string, lexer=lexer)

    # Give the batches / lists a chance to write to disk (since this isnt allowed in __del__)
    mesh_batcher.destructor()
    non_instanced_shapes.destructor()
    instances.destructor()

    return ret


if __name__ == "__main__":
    #string = r'LookAt 1 2 3 WorldBegin LightSource "infinite" WorldEnd'
    string = """
LookAt 3 4 1.5  # eye
       .5 .5 0  # look at point
       0 0 1    # up vector
Camera "perspective" "float fov" 45

Sampler "halton" "integer pixelsamples" 128
Integrator "path"
Film "image" "string filename" "simple.png"
     "integer xresolution" [400] "integer yresolution" [400]

WorldBegin

# uniform blue-ish illumination from all directions
LightSource "infinite" "rgb L" [.4 .45 .5]

# approximate the sun
LightSource "distant"  "point from" [ -30 40  100 ]
   "blackbody L" [3000 1.5]

AttributeBegin
  Material "glass"
  Shape "sphere" "float radius" 1
AttributeEnd

AttributeBegin
  Texture "checks" "spectrum" "checkerboard"
          "float uscale" [8] "float vscale" [8]
          "rgb tex1" [.1 .1 .1] "rgb tex2" [.8 .8 .8]
  Material "matte" "texture Kd" "checks"
  # Translate 0 0 -1
  Shape "trianglemesh"
      "integer indices" [0 1 2 0 2 3]
      "point P" [ -20 -20 0   20 -20 0   20 20 0   -20 20 0 ]
      "float st" [ 0 0   1 0    1 1   0 1 ]
AttributeEnd

WorldEnd
    """

    result = yacc.parse(string)
    print("Result: ", result)
