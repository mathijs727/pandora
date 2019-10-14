import pandora_py
import numpy as np
import timeit

if __name__ == "__main__":
	print("Creating string")
	s = " ".join(["1000.0" for i in range(5 * 1000 * 1000)])

	print(f"Benchmarking with string {len(s) // 1000000}MB")
	time_np = timeit.timeit(lambda: np.float32(s.split(" ")), number=1)
	time_pd = timeit.timeit(lambda: pandora_py.string_to_numpy_float(s), number=1)

	print(f"=== Time to parse ===")
	print(f" Numpy:  {time_np * 1000}ms")
	print(f" Custom: {time_pd * 1000}ms")
