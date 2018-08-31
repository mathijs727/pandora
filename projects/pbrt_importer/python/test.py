"""import pandora_py

if __name__ == "__main__":
    world = pandora_py.createWorld()
    #world.set("HELLO WORLD")
    print(world.greet())"""

import parsing
import os
import simplejson

if __name__ == "__main__":
    #path = os.path.dirname(__file__)
    path = "E:/Pandora Scenes/PBRT/pbrt-v3-scenes/sanmiguel"
    file_path = os.path.join(path, "sanmiguel.pbrt")
    pbrt_file = parsing.parse_file(file_path)
    
    #print(pbrt_file)
    with open(os.path.join(os.path.dirname(__file__), "out.json"), "w") as f:
        f.write(simplejson.dumps(pbrt_file, indent=2))

