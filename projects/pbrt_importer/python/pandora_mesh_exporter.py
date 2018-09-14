import os
import pickle
import sys
import pandora_py

# Similar to parsing.MeshBatcher but for Pandora format meshes instead of Python (pickled) meshes
class PandoraMeshExporter:
    def __init__(self, out_mesh_path):
        self._out_mesh_path = out_mesh_path
        self._file_id = 0

        self._min_batch_size = 500 * 1000 * 1000
        self._current_mesh_batch = None
        self._create_new_batch()

    def _gen_filename(self):
        ret = os.path.join(self._out_mesh_path, f"mesh{self._file_id}.bin")
        self._file_id += 1
        return ret

    def add_triangle_mesh(self, triangles, positions, normals, tangents, uv_coords):
        filename, start_byte, size_bytes = self._current_mesh_batch.addTriangleMesh(
            triangles, positions, normals, tangents, uv_coords)

        if start_byte + size_bytes > self._min_batch_size:
            self._create_new_batch()

        return (filename, start_byte, size_bytes)

    def _create_new_batch(self):
        print("Creating new Pandora mesh batch...")
        self._current_mesh_batch = pandora_py.PandoraMeshBatch(
            self._gen_filename())
