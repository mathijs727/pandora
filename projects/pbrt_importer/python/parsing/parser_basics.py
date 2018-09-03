from deepmerge import merge_or_raise, exception
from collections import namedtuple
import os

base_path = ""

Point = namedtuple("Point", ["x", "y", "z"])
Normal = namedtuple("Normal", ["x", "y", "z"])

RGB = namedtuple("RGB", ["r", "g", "b"])
XYZ = namedtuple("XYZ", ["X", "Y", "Z"])
SampledSpectrum = namedtuple("SampledSpectrum", ["values", "wavelengths_nm"])
SampledSpectrumFile = namedtuple("SampledSpectrumFile", ["filename"])
TextureRef = namedtuple("TextureRef", ["name"])
BlackBody = namedtuple("BlackBody", ["temperature_kelvin", "scale_factor"])


def p_error(p):
    print("Syntax error at %s" % repr(p))


def p_basic_data_type(p):
    """basic_data_type : STRING
                       | NUMBER"""
    p[0] = p[1]


def p_data_list_items(p):
    """data_list_items : data_list_items basic_data_type
                      | """
    if len(p) == 3:
        p[0] = p[1] + [p[2]]
    else:
        p[0] = []


def p_data_list(p):
    "data_list : L_SQUARE_BRACKET data_list_items R_SQUARE_BRACKET"
    p[0] = p[2]


def p_argument(p):
    """argument : STRING basic_data_type
                | STRING data_list"""
    global base_path
    arg_type, arg_name = p[1].split()
    data = p[2]

    # Unfold lists of single length
    if isinstance(data, list) and len(data) == 1:
        data = data[0]

    if isinstance(data, list):
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
        elif arg_type == "point":
            length = len(data)
            if length == 3:
                p[0] = {arg_name: Point(data[0], data[1], data[2])}
            else:
                assert(length % 3 == 0)
                result = []
                for i in range(0, length, 3):
                    result.append(Point(data[i], data[i+1], data[i+2]))
                p[0] = {arg_name: result}
        elif arg_type == "normal":
            length = len(data)
            if length == 3:
                p[0] = {arg_name: Normal(data[0], data[1], data[2])}
            else:
                assert(length % 3 == 0)
                result = []
                for i in range(0, length, 3):
                    result.append(Normal(data[i], data[i+1], data[i+2]))
                p[0] = {arg_name: result}
        elif arg_type == "blackbody":
            assert(len(data) == 2)
            p[0] = {
                "blackbody_temperature_kelvin": float(data[0]),
                "blackbody_scale_factor": float(data[1])
            }
        else:
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

