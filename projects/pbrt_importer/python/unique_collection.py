import numpy as np
import ujson
from itertools import chain
from parsing.file_backed_list import FileBackedList
import os
import sqlite3

class OfflineLUT:
    def __init__(self, filename):
        if os.path.exists(filename):
            os.remove(filename)
        
        self._filename = filename
        self._conn = sqlite3.connect(filename)
        self._conn.execute("CREATE TABLE lut (key TEXT PRIMARY KEY, id INTEGER)")

    def __contains__(self, key):
        return self._conn.execute("SELECT * FROM lut WHERE key=?", (key,)).fetchone() is not None
        
    def __setitem__(self, key, value):
        self._conn.execute("INSERT INTO lut VALUES (?, ?)", (key,value))

    def __getitem__(self, key):
        try:
            return self._conn.execute("SELECT id FROM lut WHERE key=?", (key,)).fetchone()[0]
        except:
            raise KeyError()

    def __del__(self):
        self.clear()

    def clear(self):
        if self._conn is not None:
            self._conn.close()
            self._conn = None

        if os.path.exists(self._filename):
            os.remove(self._filename)

class UniqueCollection:
    def __init__(self, tmp_folder):
        if not os.path.exists(tmp_folder):
            os.makedirs(tmp_folder)

        self._list = FileBackedList(tmp_folder)
        self._dict = OfflineLUT(os.path.join(tmp_folder, "lut.db"))
        self._current_index = 0

    def add_item(self, item):
        if isinstance(item, dict):
            try:
                #key = json.dumps(item, cls=NumpyEncoder)
                key = ujson.dumps(item)
            except TypeError as e:
                print(e)
                print(item)
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

    def get_list(self):
        self._list.destructor()
        return [i for i in self._list]

    def __iter__(self):
        self._list.finish_chunk()
        return self._list.__iter__()


if __name__ == "__main__":
    collection = UniqueCollection("asdf")
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item({"a": "c"}) == 1)
    assert(collection.add_item({"b": "x"}) == 2)
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item(123.0) == 3)
