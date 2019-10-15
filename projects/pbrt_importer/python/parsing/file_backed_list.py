import os
import pickle


class FileBackedList:
    def __init__(self, folder, chunk_size=50*1024*1024):
        self._folder = folder
        self._chunk_size = chunk_size

        self._current_chunk_id = 0
        self._current_chunk = []

        if not os.path.exists(self._folder):
            os.makedirs(self._folder)

    def append(self, item):
        self._current_chunk.append(item)
        if len(self._current_chunk) >= self._chunk_size:
            self._write_chunk()

    def _write_chunk(self):
        filename = os.path.join(
            self._folder, f"list{self._current_chunk_id}.bin")
        with open(filename, "wb") as f:
            pickle.dump(self._current_chunk, f)

        self._current_chunk = []
        self._current_chunk_id += 1

    def finish_chunk(self):
        if len(self._current_chunk) > 0:
            self._write_chunk()

    def destructor(self):
        self.finish_chunk()

    def __iter__(self):
        self.finish_chunk()
        return FileBackedList_iter(self._folder)


class FileBackedList_iter:
    def __init__(self, folder):
        self._folder = folder
        self._current_chunk_id = 0
        self._current_index = 0
        self._current_chunk = []

        # self._read_next_chunk()

    def __iter__(self):
        return self

    def _read_next_chunk(self):
        filename = os.path.join(
            self._folder, f"list{self._current_chunk_id}.bin")
        if not os.path.exists(filename):
            raise StopIteration()

        with open(filename, "rb") as f:
            self._current_chunk = pickle.load(f)

        self._current_chunk_id += 1
        self._current_index = 0

    def __next__(self):
        if self._current_index >= len(self._current_chunk):
            self._read_next_chunk()

        ret = self._current_chunk[self._current_index]
        self._current_index += 1
        return ret
