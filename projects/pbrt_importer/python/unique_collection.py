import json
import numpy as np


class Float32Encoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.float32):
            return float(obj)
        return json.JSONEncoder.default(self, obj)


class UniqueCollection:
    def __init__(self):
        self._list = []
        self._dict = {}

    def add_item(self, item):
        if isinstance(item, dict):
            try:
                key = json.dumps(item, cls=Float32Encoder)
            except TypeError as e:
                print(e)
                print(item)
        else:
            key = item

        if key in self._dict:
            return self._dict[key]
        else:
            index = len(self._list)
            self._list.append(item)
            self._dict[key] = index
            return index

    def get_list(self):
        return self._list


if __name__ == "__main__":
    collection = UniqueCollection()
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item({"a": "c"}) == 1)
    assert(collection.add_item({"b": "x"}) == 2)
    assert(collection.add_item({"a": "b"}) == 0)
    assert(collection.add_item(123.0) == 3)
