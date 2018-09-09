import pandora_py
import time
import numpy as np

print("Creating string...")
string = " ".join(["1.0" for i in range(10000)])
#string = "	1.0   2.0		0.2334 "

print("Calling C++")
start = time.time()
x = pandora_py.string_to_numpy_float(string)
end = time.time()
print(f"C++ implementation: {(end - start) * 1000}ms")

print("Calling numpy")
start = time.time()
y = np.fromstring(string, dtype=float, sep=' ')
end = time.time()
print(f"numpy fromstring: {(end - start) * 1000}ms")

print("Same result: ", ((x == y).all()))