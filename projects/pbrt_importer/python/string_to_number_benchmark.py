import pandora_py
import numpy as np
import timeit

def benchmark(s, f):
	time_np = 0#timeit.timeit(lambda: np.float32(s.split(" ")), number=1)
	time_pd = timeit.timeit(lambda: f(s), number=1)

	print(f"=== Time to parse ===")
	print(f" Numpy:  {time_np * 1000}ms")
	print(f" Custom: {time_pd * 1000}ms")

if __name__ == "__main__":
	print("Float")
	s = " ".join(["1000.0" for i in range(20 * 1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_float)

	print("\nDouble")
	s = " ".join(["1000.0" for i in range(20 * 1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_double)

	print("\nInteger")
	s = " ".join(["123423" for i in range(20 * 1000 * 1000)])
	benchmark(s, pandora_py.string_to_numpy_int)


