import pandora_py
import time
import numpy as np
import gc

for s in range(100000):
	string = " ".join([str(i) for i in range(s)])

	x = pandora_py.string_to_numpy_int(string)
	y = np.fromstring(string, dtype=int, sep=' ')

	if not (x == y).all():
		print("Different results!")
		assert(False)

"""print("==== FLOATS ====")
print("Creating string...")
string = " ".join(["1.0" for i in range(10000000)])

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



print("\n\n")
print("==== INTEGERS ====")
print("Creating string...")
string = " ".join([str(i) for i in range(10000000)])

print("Calling C++")
start = time.time()
x = pandora_py.string_to_numpy_int(string)
end = time.time()
print(f"C++ implementation: {(end - start) * 1000}ms")

print("Calling numpy")
start = time.time()
y = np.fromstring(string, dtype=int, sep=' ')
end = time.time()
print(f"numpy fromstring: {(end - start) * 1000}ms")

print("Same result: ", ((x == y).all()))"""