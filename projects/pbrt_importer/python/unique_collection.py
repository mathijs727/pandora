class UniqueCollection:
    def __init__(self):
        self._list = []
        self._dict = {}

    def __getitem__(self, item):
        try:
            return self._dict[item]
        except KeyError:
            index = len(self._list)
            self._list.append(item)
            self._dict[item] = index
            return index

    def get_items(self):
        return self._list
