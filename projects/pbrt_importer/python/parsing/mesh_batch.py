import os
import pickle
import sys

class MeshBatcher:
    def __init__(self, out_mesh_path):
        self._out_mesh_path = out_mesh_path
        self._new_mesh_id = 0
        self._current_mesh_filename = ""

        self._max_batch_size = 500 * 1000 * 1000
        self._file = None
        self._create_new_batch()

    def _gen_filename(self):
        ret = os.path.join(self._out_mesh_path, f"mesh{self._new_mesh_id}.bin")
        self._new_mesh_id += 1
        return ret

    def add_mesh(self, data):
        filename = self._current_mesh_filename
        start_byte = self._file.tell()
        pickle.dump(data, self._file, protocol=4)
        num_bytes = self._file.tell() - start_byte

        if sys.getsizeof(start_byte + num_bytes) > self._max_batch_size:
            self._create_new_batch()

        return (filename, start_byte, num_bytes)

    # NOTE: not an actual destructor (user has to call it). Using a real destructor (__del__) is impossible because
    #  writing files from the __del__ function is not supported.
    # https://stackoverflow.com/questions/23422188/why-am-i-getting-nameerror-global-name-open-is-not-defined-in-del
    # 
    # Manually calling the destructor ensures that we won't try to read from the file while its still opened (because
    #  garbage collection has not deleted the mesh batcher object yet).
    def destructor(self):
        self._file.close()

    def _create_new_batch(self):
        print("Creating new geometry batch...")

        if self._file:
            self._file.close()
        self._current_mesh_filename = self._gen_filename()
        self._file = open(self._current_mesh_filename, "wb")
