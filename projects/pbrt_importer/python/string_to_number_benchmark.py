import pandora_py
import numpy as np
import timeit

def benchmark(s, f, dtype):
	time_np1 = timeit.timeit(lambda: np.fromstring(s, dtype=dtype, sep=' '), number=1)
	time_np2 = timeit.timeit(lambda: dtype(s.split(" ")), number=1)
	time_pd = timeit.timeit(lambda: f(s), number=1)

	print(f"=== Time to parse ===")
	print(f" Numpy fromstring: {time_np1 * 1000}ms")
	print(f" Numpy list:       {time_np2 * 1000}ms")
	print(f" Custom:           {time_pd * 1000}ms")

if __name__ == "__main__":
	print("Float")
	s = " ".join(["1000.0" for i in range(1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_float, np.float32)

	print("\nDouble")
	s = " ".join(["1000.0" for i in range(1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_double, np.float64)

	print("\nInteger")
	s = " ".join(["123423" for i in range(1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_int32, np.int32)


