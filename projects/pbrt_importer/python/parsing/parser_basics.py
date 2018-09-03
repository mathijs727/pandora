from deepmerge import merge_or_raise, exception
from collections import namedtuple
import numpy as np
import os

base_path = ""
current_file = ""

Point = namedtuple("Point", ["x", "y", "z"])
Point2 = namedtuple("Point2", ["x", "y"])
Normal = namedtuple("Normal", ["x", "y", "z"])
Normal2 = namedtuple("Normal2", ["x", "y"])
Vector = namedtuple("Vector", ["x", "y", "z"])
Vector2 = namedtuple("Vector2", ["x", "y"])

RGB = namedtuple("RGB", ["r", "g", "b"])
XYZ = namedtuple("XYZ", ["X", "Y", "Z"])
SampledSpectrum = namedtuple("SampledSpectrum", ["values", "wavelengths_nm"])
SampledSpectrumFile = namedtuple("SampledSpectrumFile", ["filename"])
TextureRef = namedtuple("TextureRef", ["name"])
BlackBody = namedtuple("BlackBody", ["temperature_kelvin", "scale_factor"])


def p_error(p):
    print(
        f"Syntax error: unexpected token \"{p.value}\" in file \"{current_file}\" at line {p.lineno}")


def p_basic_data_type(p):
    """basic_data_type : STRING
                       | NUMBER"""
    p[0] = p[1]
    lineno = p.lineno(n=1)
    if lineno % 1000 == 0:
        print(f"Parsed data at line {lineno}")


def p_list(p):
    "list : LIST"
    print("LIST")
    text = p[1][1:-1]
    if '"' in text:
        import re
        p[0] = re.findall('"[^"]*"', text)
    elif '.' in text:
        result = np.fromstring(text, dtype=float, sep=' ')
        p[0] =result
    else:
        result = np.fromstring(text, dtype=int, sep=' ')
        p[0] = result
    print("OUTLIST")


def p_argument(p):
    """argument : STRING basic_data_type
                | STRING list"""
    global base_path
    arg_type, arg_name = p[1].split()
    data = p[2]

    # Unfold lists of single length
    if (isinstance(data, list) or isinstance(data, np.ndarray)) and len(data) == 1:
        data = data[0]

    if isinstance(data, list) or isinstance(data, np.ndarray):
        if arg_type == "integer":
            p[0] = {arg_name: [int(x) for x in data]}
        elif arg_type == "float":
            p[0] = {arg_name: [float(x) for x in data]}
        elif arg_type == "rgb" or arg_type == "color":
            p[0] = {arg_name: RGB(data[0], data[1], data[2])}
        elif arg_type == "xyz":
            p[0] = {arg_name: XYZ(data[0], data[1], data[2])}
        elif arg_type == "spectrum":
            assert(len(data) % 2 == 0)
            p[0] = {arg_name: SampledSpectrum(
                values=data[0::2], wavelengths_nm=data[1::2])}
        elif arg_type == "point2":
            length = len(data)
            if length == 2:
                p[0] = {arg_name: Point2(data[0], data[1])}
            else:
                assert(length % 2 == 0)
                result = []
                for i in range(0, length, 2):
                    result.append(Point2(data[i], data[i+1]))
                p[0] = {arg_name: result}
        elif arg_type == "point3" or arg_type == "point":
            length = len(data)
            if length == 3:
                p[0] = {arg_name: Point(data[0], data[1], data[2])}
            else:
                assert(length % 3 == 0)
                result = []
                for i in range(0, length, 3):
                    result.append(Point(data[i], data[i+1], data[i+2]))
                p[0] = {arg_name: result}
        elif arg_type == "normal2":
            length = len(data)
            if length == 2:
                p[0] = {arg_name: Normal2(data[0], data[1])}
            else:
                assert(length % 2 == 0)
                result = []
                for i in range(0, length, 2):
                    result.append(Normal2(data[i], data[i+1]))
                p[0] = {arg_name: result}
        elif arg_type == "normal3" or arg_type == "normal":
            length = len(data)
            if length == 3:
                p[0] = {arg_name: Normal(data[0], data[1], data[2])}
            else:
                assert(length % 3 == 0)
                result = []
                for i in range(0, length, 3):
                    result.append(Normal(data[i], data[i+1], data[i+2]))
                p[0] = {arg_name: result}
        elif arg_type == "vector2":
            length = len(data)
            if length == 2:
                p[0] = {arg_name: Vector2(data[0], data[1])}
            else:
                assert(length % 2 == 0)
                result = []
                for i in range(0, length, 2):
                    result.append(Vector2(data[i], data[i+1]))
                p[0] = {arg_name: result}
        elif arg_type == "vector3" or arg_type == "vector":
            length = len(data)
            if length == 3:
                p[0] = {arg_name: Vector(data[0], data[1], data[2])}
            else:
                assert(length % 3 == 0)
                result = []
                for i in range(0, length, 3):
                    result.append(Vector(data[i], data[i+1], data[i+2]))
                p[0] = {arg_name: result}
        elif arg_type == "blackbody":
            assert(len(data) == 2)
            p[0] = {
                "blackbody_temperature_kelvin": float(data[0]),
                "blackbody_scale_factor": float(data[1])
            }
        else:
            print(data)
            raise RuntimeError(f"Unknown argument type {arg_type}")
    else:
        if arg_type == "integer":
            p[0] = {arg_name: int(data)}
        elif arg_type == "float":
            p[0] = {arg_name: float(data)}
        elif arg_type == "bool":
            p[0] = {arg_name: True if data == "true" else False}
        elif arg_type == "string":
            p[0] = {arg_name: str(data)}
        elif arg_type == "texture":
            p[0] = {arg_name: TextureRef(data)}
        elif arg_type == "spectrum":
            p[0] = {arg_name: SampledSpectrumFile(
                os.path.join(base_path, data))}
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
