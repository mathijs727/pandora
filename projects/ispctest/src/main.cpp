#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
//#include <execution>

#include "ispc_transform_ispc.h"
using namespace ispc;

int main()
{
	int N = 10000000;
	std::vector<int> a(N);
	std::vector<int> b(N);
	std::vector<int> c(N);
	std::fill(std::begin(a), std::end(a), 42);
	std::fill(std::begin(b), std::end(b), 38);

	using clock = std::chrono::high_resolution_clock;
	auto start = clock::now();

	//std::transform(std::execution::seq, std::begin(a), std::end(a), std::begin(b), std::begin(c), [](int x, int y) { return x + y; });
	performSum(a.data(), b.data(), c.data(), N);

	auto end = clock::now();
	auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	std::cout << "Time to sum: " << timeDelta.count() / 1000.0f << "ms" << std::endl;

	for (int v : c)
	{
		if (v != 80)
			throw std::runtime_error("Incorrect result!");
	}

	//for (int v : c)
	//	std::cout << v << " ";
	return 0;
}