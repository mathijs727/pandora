import pandora_py
import time
import numpy as np
import gc

print("START")

def test_correctness():
    for s in range(100000):
        string = " ".join([str(i) for i in range(s)])

        x = pandora_py.string_to_numpy_int32(string)
        y = np.fromstring(string, dtype=np.int32, sep=' ')

        if not (x == y).all():
            print("Different results!")
            assert(False)

def perform_timings():
    print("==== FLOATS ====")
    print("Creating string...")
    string = " ".join(["1.0" for i in range(100000)])

    print("Calling C++")
    start = time.time()
    x = pandora_py.string_to_numpy_float(string)
    end = time.time()
    print(f"C++ implementation: {(end - start) * 1000}ms")

    print("Calling numpy fromstring")
    start = time.time()
    y = np.fromstring(string, dtype=np.float32, sep=' ')
    end = time.time()
    print(f"numpy fromstring: {(end - start) * 1000}ms")

    print("Calling numpy np.float32(str.split())")
    start = time.time()
    z = np.float32(string.split(" "))
    end = time.time()
    print(f"numpy fromstring: {(end - start) * 1000}ms")

    print(f"Same C++ / np.fromstring:           {(x == y).all()}")
    print(f"Same C++ / np.float32(str.split()): {(x == z).all()}")



    print("\n\n")
    print("==== INTEGERS ====")
    print("Creating string...")
    string = " ".join([str(i) for i in range(100000)])

    print("Calling C++")
    start = time.time()
    x = pandora_py.string_to_numpy_int32(string)
    end = time.time()
    print(f"C++ implementation: {(end - start) * 1000}ms")

    print("Calling numpy")
    start = time.time()
    y = np.fromstring(string, dtype=np.int32, sep=' ')
    end = time.time()
    print(f"numpy fromstring: {(end - start) * 1000}ms")

    print("Calling numpy")
    start = time.time()
    z = np.int32(string.split(" "))
    end = time.time()
    print(f"numpy fromstring: {(end - start) * 1000}ms")

    print(f"Same C++ / np.fromstring:           {(x == y).all()}")
    print(f"Same C++ / np.float32(str.split()): {(x == z).all()}")

if __name__ == "__main__":
    perform_timings()
    