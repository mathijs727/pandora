"""import pandora_py

if __name__ == "__main__":
    world = pandora_py.createWorld()
    #world.set("HELLO WORLD")
    print(world.greet())"""

import parsing
import os
import simplejson
import pickle

if __name__ == "__main__":
    #path = "E:/Pandora Scenes/PBRT/island-pbrt-v1/island/pbrt/"
    #path = "/mnt/e/Pandora Scenes/PBRT/island-pbrt-v1/island/pbrt/"
    #file_path = os.path.join(path, "island.pbrt")

    if os.name == "nt":
        path = "E:/Pandora Scenes/PBRT/pbrt-v3-scenes/breakfast"
    else:
        path = "/mnt/e/Pandora Scenes/PBRT/pbrt-v3-scenes/breakfast"
    file_path = os.path.join(path, "breakfast.pbrt")
    pbrt_file = parsing.parse_file(file_path)
    
    print("Writing out pickle binary")
    with open(os.path.join(os.path.dirname(__file__), "out.bin"), "wb") as f:
        f.write(pickle.dumps(pbrt_file))

    print("Writing out JSON")
    with open(os.path.join(os.path.dirname(__file__), "out.json"), "w") as f:
        f.write(simplejson.dumps(pbrt_file, indent=2))


