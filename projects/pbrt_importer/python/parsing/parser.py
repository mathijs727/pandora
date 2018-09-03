from deepmerge import merge_or_raise
import numpy as np
import os
import sys
from collections import namedtuple
from enum import Enum
import pickle
from parsing.matrix import translate, scale, rotate, lookat
from parsing.lexer import *
import parsing.lexer
import parsing.parser_basics
import ply.yacc as yacc

class ParsingState(Enum):
    CONFIG = 1
    SCENE = 2


parsing_state = ParsingState.CONFIG


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

# Putting this at the top of the file confuses PLY for some weird reason
base_path = ""
out_mesh_path= ""
new_mesh_id = 0

def p_statement_include(p):
    "statement_include : INCLUDE STRING"
    global parsing_state, base_path


    # Store state
    current_file_bak = parsing.lexer.current_file

    # Set new state
    include_file = os.path.join(base_path, p[2])
    parsing.lexer.current_file = include_file
    parsing.parser_basics.current_file = include_file

    print(f"Processing include file {include_file}")
    with open(include_file, "r") as f:
        lexer = lex.lex(optimize=True)
        if parsing_state == ParsingState.CONFIG:
            parser = yacc.yacc(start="statements_config")
        else:
            parser = yacc.yacc(start="statements_scene")
        return parser.parse(f.read(), lexer=lexer)


    # Restore state
    parsing.lexer.current_file = current_file_bak
    parsing.parser_basics.current_file = current_file_bak

    return None


def p_statements_scene_include(p):
    """statement_scene : statement_include"""
    p[0] = p[1]


def p_statements_config_include(p):
    """statement_config : statement_include"""
    p[0] = p[1]


# For some reason PLY fails if this is imported at the top of this file
from parsing.parser_basics import *

Camera = namedtuple("Camera", ["type", "arguments", "transform"])
LightSource = namedtuple("LightSource", ["type", "arguments", "transform"])
AreaLightSource = namedtuple("AreaLightSource", ["type", "arguments"])
Shape = namedtuple("Shape", ["type", "arguments",
                             "transform", "flip_normals", "material", "area_light"])
InstanceTemplate = namedtuple(
    "InstanceTemplate", ["name", "shapes", "transform"])
Instance = namedtuple(
    "Instance", ["template_name", "transform"])
Material = namedtuple("Material", ["type", "arguments"])
Texture = namedtuple("Texture", ["name", "type", "texture_class", "arguments"])

named_materials = {}
named_textures = {}
light_sources = []
instance_templates = {}
instances = []
non_instanced_shapes = []

default_material = Material(type="matte", arguments={"Kd": RGB(1.0, 1.0, 1.0)})
graphics_state_stack = []
graphics_state = {"area_light": None,
                  "flip_normals": False, "material": default_material}
transform_stack = []
named_transforms = {}
cur_transform = np.identity(4)
current_instance = None


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
    cur_transform = np.matmul(translate(xyz), cur_transform)


def p_statement_transform_scale(p):
    "statement_transform : SCALE NUMBER NUMBER NUMBER"
    global cur_transform
    xyz = np.array([p[2], p[3], p[4]])
    cur_transform = np.matmul(scale(xyz), cur_transform)


def p_statement_transform_rotate(p):
    "statement_transform : ROTATE NUMBER NUMBER NUMBER NUMBER"
    global cur_transform
    angle = p[2]
    xyz = np.array([p[3], p[4], p[5]])
    cur_transform = np.matmul(rotate(angle, xyz), cur_transform)


def p_statement_transform_lookat(p):
    "statement_transform : LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER"
    global cur_transform
    eye = np.array([p[2], p[3], p[4]])
    look = np.array([p[5], p[6], p[7]])
    up = np.array([p[8], p[9], p[10]])
    cur_transform = np.matmul(lookat(eye, look, up), cur_transform)


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
    matrix = np.array(p[2]).reshape((4, 4))
    cur_transform = np.matmul(matrix, cur_transform)


def p_statement_transform_concat(p):
    "statement_transform : CONCAT_TRANSFORM list"
    #"statement_transform : CONCAT_TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET"
    global cur_transform
    matrix = np.array(p[2]).reshape((4, 4))
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
        print(f"WARNING: named material {material_name} was not declared! Ignoring...")


def p_statement_make_named_material(p):
    "statement_scene : MAKE_NAMED_MATERIAL STRING arguments"
    global named_materials
    arguments = p[3]
    material_type = arguments["type"]
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
            arguments["filename"] = os.path.join(
                base_path, arguments["filename"])
    named_textures[p[2]] = Texture(
        name=p[2], type=p[3], texture_class=p[4], arguments=arguments)


def p_statement_light(p):
    "statement_scene : LIGHT_SOURCE STRING arguments"
    global light_sources, cur_transform
    light_type = p[2]
    arguments = p[3]
    light_sources.append(LightSource(
        light_type, arguments, cur_transform.tolist()))


def p_statement_area_light(p):
    "statement_scene : AREA_LIGHT_SOURCE STRING arguments"
    global graphics_state
    light_type = p[2]
    arguments = p[3]
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
        arguments["filename"] = os.path.join(base_path, arguments["filename"])
    else:
        global out_mesh_path, new_mesh_id
        new_mesh_path = os.path.join(out_mesh_path, f"pbrt_mesh_{new_mesh_id}.bin")
        new_mesh_id += 1

        with open(new_mesh_path, "wb") as f:
            f.write(pickle.dumps(arguments))

        arguments = {"filename": new_mesh_path}

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
        "camera": Camera(type=camera_type, arguments=arguments, transform=cur_transform.tolist())
    }


def p_statement_sampler(p):
    "statement_config : SAMPLER STRING arguments"
    sampler_type = p[2]
    arguments = p[3]
    # A PRBTv3 scene file may contain multiple samplers (ie the Crown scene)
    p[0] = {"samplers": [merge_or_raise.merge(
        {"type": sampler_type}, arguments)]}


def p_statement_film(p):
    "statement_config : FILM STRING arguments"
    film_type = p[2]
    arguments = p[3]
    p[0] = {"films": [merge_or_raise.merge({"type": film_type}, arguments)]}


def p_statement_filter(p):
    "statement_config : FILTER STRING arguments"
    filter_type = p[2]
    arguments = p[3]
    p[0] = {"filter": merge_or_raise.merge({"type": filter_type}, arguments)}


def p_statement_integrator(p):
    "statement_config : INTEGRATOR STRING arguments"
    integrator_type = p[2]
    arguments = p[3]
    p[0] = {
        "integrator": merge_or_raise.merge({"type": integrator_type}, arguments)
    }


def p_statement_accelerator(p):
    "statement_config : ACCELERATOR STRING arguments"
    accelerator_type = p[2]
    arguments = p[3]
    p[0] = {
        "accelerator": merge_or_raise.merge({"type": accelerator_type}, arguments)
    }


def parse_file(file_path):
    import re
    lexer = lex.lex(optimize=True)
    parser = yacc.yacc()

    import parsing.lexer
    parsing.lexer.current_file = file_path

    with open(file_path, "r") as f:
        string = f.read()

    global base_path, out_mesh_path
    out_mesh_path = os.path.join(os.path.abspath(os.getcwd()), "pbrt_meshes")
    if not os.path.exists(out_mesh_path):
        os.makedirs(out_mesh_path)

    current_file = os.path.abspath(file_path)
    base_path = os.path.dirname(current_file)
    parsing.parser_basics.base_path = base_path
    parsing.lexer.current_file = current_file
    parsing.parser_basics.current_file = current_file

    print("Base path: ", base_path)

    print("Parsing...")
    return yacc.parse(string, lexer=lexer)


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
