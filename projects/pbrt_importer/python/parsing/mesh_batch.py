import os
import pickle
import sys

class MeshBatcher:
    def __init__(self, out_mesh_path):
        self._out_mesh_path = out_mesh_path
        self._new_mesh_id = 0
        self._current_mesh_filename = self._gen_filename()

        self._max_batch_size = 500 * 1000 * 1000
        self._data = bytearray()

    def _gen_filename(self):
        ret = os.path.join(self._out_mesh_path, f"mesh{self._new_mesh_id}.bin")
        self._new_mesh_id += 1
        return ret

    def add_mesh(self, data):
        filename = self._current_mesh_filename
        start_byte = len(self._data)
        self._data.extend(pickle.dumps(data))
        num_bytes = len(self._data) - start_byte

        if sys.getsizeof(self._data) > self._max_batch_size:
            self._write_batch()

        return (filename, start_byte, num_bytes)

    # NOTE: not an actual destructor (user has to call it). Using a real destructor (__del__) is impossible because
    #  writing files from the __del__ function is not supported.
    # https://stackoverflow.com/questions/23422188/why-am-i-getting-nameerror-global-name-open-is-not-defined-in-del
    def destructor(self):
        self._write_batch()

    def _write_batch(self):
        print("Writing geometry batch...")

        with open(self._current_mesh_filename, "wb") as f:
            f.write(self._data)
        
        self._data.clear()
        self._current_mesh_filename = self._gen_filename()
