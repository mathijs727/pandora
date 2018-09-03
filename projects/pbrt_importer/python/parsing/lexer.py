import ply.lex as lex

current_file = ""

tokens = (
    "WORLD_BEGIN", "WORLD_END",
    "CAMERA", "SAMPLER", "INTEGRATOR", "FILM", "FILTER", "ACCELERATOR",
    "STRING", "NUMBER",

    "ATTRIBUTE_BEGIN", "ATTRIBUTE_END", "TRANSFORM_BEGIN", "TRANSFORM_END", "OBJECT_BEGIN", "OBJECT_END",
    "SHAPE", "OBJECT_INSTANCE", "MATERIAL", "NAMED_MATERIAL", "MAKE_NAMED_MATERIAL", "TEXTURE",
    "LIGHT_SOURCE", "AREA_LIGHT_SOURCE",

    "COMMENT", "L_SQUARE_BRACKET", "R_SQUARE_BRACKET", "INCLUDE",

    "IDENTITY", "TRANSLATE", "SCALE", "ROTATE", "LOOKAT",
    "COORDINATE_SYSTEM", "COORDINATE_SYSTEM_TRANSFORM", "TRANSFORM", "CONCAT_TRANSFORM", "REVERSE_ORIENTATION"
)

# Tokens
t_WORLD_BEGIN = r'WorldBegin'
t_WORLD_END = r'WorldEnd'
t_CAMERA = r'Camera'
t_SAMPLER = r'Sampler'
t_INTEGRATOR = r'Integrator'
t_FILM = r'Film'
t_FILTER = r'PixelFilter'
t_ACCELERATOR = r'Accelerator'

t_ATTRIBUTE_BEGIN = r'AttributeBegin'
t_ATTRIBUTE_END = r'AttributeEnd'
t_TRANSFORM_BEGIN = r'TransformBegin'
t_TRANSFORM_END = r'TransformEnd'
t_OBJECT_BEGIN = r'ObjectBegin'
t_OBJECT_END = r'ObjectEnd'
t_SHAPE = r'Shape'
t_OBJECT_INSTANCE = r'ObjectInstance'
t_MATERIAL = r'Material'
t_NAMED_MATERIAL = r'NamedMaterial'
t_MAKE_NAMED_MATERIAL = r'MakeNamedMaterial'
t_TEXTURE = r'Texture'
t_LIGHT_SOURCE = r'LightSource'
t_AREA_LIGHT_SOURCE = r'AreaLightSource'

t_L_SQUARE_BRACKET = '\['
t_R_SQUARE_BRACKET = '\]'
t_INCLUDE = r'Include'

t_IDENTITY = r'Identity'
t_TRANSLATE = r'Translate'
t_SCALE = r'Scale'
t_ROTATE = r'Rotate'
t_LOOKAT = r'LookAt'
t_COORDINATE_SYSTEM = r'CoordinateSystem'
t_COORDINATE_SYSTEM_TRANSFORM = r'CoordSysTransform'
t_TRANSFORM = r'Transform'
t_CONCAT_TRANSFORM = r'ConcatTransform'
t_REVERSE_ORIENTATION = r'ReverseOrientation'


def t_STRING(t):
    r'"[^"]*"'
    t.value = t.value[1:-1]
    return t


def t_NUMBER(t):
    # https://www.regular-expressions.info/floatingpoint.html
    r'[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?'
    if "." in t.value or "e+" in t.value:
        t.value = float(t.value)
    else:
        t.value = int(t.value)
    return t


def t_COMMENT(t):
    r'\#.*'
    return None


# http://www.dabeaz.com/ply/ply.html#ply_nn9
def t_newline(t):
    r'\n+'
    t.lexer.lineno += len(t.value)

# http://www.dabeaz.com/ply/ply.html#ply_nn9
# Compute column.
#     input is the input text string
#     token is a token instance
def find_column(input, token):
    line_start = input.rfind('\n', 0, token.lexpos) + 1
    return (token.lexpos - line_start) + 1


def t_error(t):
    global current_file
    column = find_column(t.lexer.lexdata, t)
    print("Illegal character '{}' in file {} at ({}: {})".format(t.value[0], current_file, t.lexer.lineno, column))
    t.lexer.skip(1)


# Ignored characters
t_ignore = " \t"
