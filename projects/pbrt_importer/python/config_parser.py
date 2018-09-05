from parsing.parser import Texture
from unique_collection import UniqueCollection
import numpy as np


class ConfigParser:
    def __init__(self, pbrt_config):
        self._camera = self._get_camera(pbrt_config)

    def _get_camera(self, pbrt_config):
        assert(len(pbrt_config["films"]) == 1)
        film = pbrt_config["films"][0]
        resolution = (film["arguments"]["xresolution"]["value"],
                      film["arguments"]["yresolution"]["value"])
        
        camera = pbrt_config["camera"]
        assert(camera.type == "perspective")
        camera_type = camera.type
        fov = camera.arguments["fov"]["value"]
        camera_to_world_transform = np.linalg.inv(np.array(camera.world_to_camera_transform).reshape(4,4)).tolist()

        return {
            "type": camera_type,
            "resolution": resolution,
            "fov": fov,
            "camera_to_world_transform": camera_to_world_transform
        }

    def data(self):
        return {
            "camera": self._camera
        }
