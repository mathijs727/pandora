import numpy as np
import ujson
from itertools import chain
from parsing.file_backed_list import FileBackedList
import os
import sqlite3
from sqlitedict import SqliteDict

class UniqueCollection:
    def __init__(self, tmp_folder):
        if not os.path.exists(tmp_folder):
            os.makedirs(tmp_folder)

        self._list = FileBackedList(tmp_folder)
        self._dict = SqliteDict(autocommit=True)#OfflineLUT(os.path.join(tmp_folder, "lut.db"))
        self._current_index = 0

    def add_item(self, item):
        if isinstance(item, dict):
            key = ujson.dumps(item)
        elif isinstance(item, list):
            key = ujson.dumps(item)
        elif isinstance(item, np.ndarray):
            key = ujson.dumps(item)
        else:
            key = item

        if key in self._dict:
            return self._dict[key]
        else:
            index = self._current_index
            self._current_index += 1
            self._list.append(item)
            self._dict[key] = index
            return index

    def finish(self):
        self._dict.clear()# Save a ton of memory when we're done with adding items
        self._list.finish_chunk()

    def to_list(self):
        self._list.finish_chunk()
        return self._list

    def __iter__(self):
        self.finish()
        return self._list.__iter__()


if __name__ == "__main__":
    collection = UniqueCollection("asdf")
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item({"a": "c"}) == 1)
    assert(collection.add_item({"b": "x"}) == 2)
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item(123.0) == 3)
