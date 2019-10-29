#include "pbrt/util/crack_atof.h"
#include "pbrt/util/crack_atof_sse.h"
#include "pbrt/util/crack_atof_avx2.h"
#include "pbrt/util/crack_atof_avx512.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

void testValidity()
{
	//std::random_device rd;
	std::mt19937 e(1234);
	std::uniform_real_distribution<float> dist(-10000, 10000);

	for (int i = 0; i < 100000; i++) {
		const float actualValue = dist(e);
		std::string string = std::to_string(actualValue);

		const float parsedValue = crack_atof_sse(string);
		assert(std::abs(parsedValue - actualValue) < 0.001f);
	}
}

int main()
{
	using clock = std::chrono::high_resolution_clock;

	testValidity();

	auto start = clock::now();
	//auto result = stringToVector<double>(myString);
	for (size_t i = 0; i < 1000 * 1000 * 1000; i++) {
		std::string_view string = "0.6799910069";
		//float y = stringToVector<float>(string);
		//float y = static_cast<float>(crackAtof(string));
		float y = crack_atof_sse(string);
		//float y = crack_atof_avx2(string);
		//float y = crack_atof_avx512(string);
	}
	auto end = clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << "Parsing took " << duration.count() << "ms" << std::endl;
	return 0;
}