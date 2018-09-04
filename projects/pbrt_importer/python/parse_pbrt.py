import parsing
import os
import pickle
import numpy as np

if __name__ == "__main__":
    if os.name == "nt":
        path = "E:/Pandora Scenes/PBRT/island-pbrt-v1/island/pbrt/"
    else:
        path = "/mnt/e/Pandora Scenes/PBRT/island-pbrt-v1/island/pbrt/"
    file_path = os.path.join(path, "island.pbrt")

    if os.name == "nt":
        path = "E:/Pandora Scenes/PBRT/pbrt-v3-scenes/breakfast/"
    else:
        path = "/mnt/e/Pandora Scenes/PBRT/pbrt-v3-scenes/breakfast"
    file_path = os.path.join(path, "breakfast.pbrt")

    pbrt_file = parsing.parse_file(file_path)

    print("Writing out pickle binary")
    with open(os.path.join(os.getcwd(), "pbrt.bin"), "wb") as f:
        f.write(pickle.dumps(pbrt_file))

    #print("Writing out JSON")
    #with open(os.path.join(os.getcwd(), "out.json"), "w") as f:
    #    f.write(json.dumps(pbrt_file, cls=MyEncoder, indent=2))
