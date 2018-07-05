#pragma once
#include <iostream>
#include <string_view>

namespace pandora
{

inline void THROW_ERROR(std::string_view errorMessage)
{
	std::cerr << errorMessage << std::endl;
	int i;
	std::cin >> i;
	(void)i;
	exit(-1);
}

inline void LOG_INFO(std::string_view infoMessage)
{
	std::cerr << infoMessage << std::endl;
}

inline void LOG_WARNING(std::string_view warningMessage)
{
	std::cerr << warningMessage << std::endl;
}

inline void ALWAYS_ASSERT(bool value, std::string_view errorMessage)
{
	if (!value) {
		THROW_ERROR(errorMessage);
	}
}

}
