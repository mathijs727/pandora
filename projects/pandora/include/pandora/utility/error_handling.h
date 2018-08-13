#pragma once
#include <iostream>
#include <string_view>

namespace pandora
{

#define THROW_ERROR(message) __THROW_ERROR(message, __FILE__, __LINE__)
inline void __THROW_ERROR(std::string_view errorMessage, std::string_view fileName, int line)
{
	std::cerr << fileName << ":" << line << " ==> " << errorMessage << std::endl;
	throw std::runtime_error("");
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

inline void ALWAYS_ASSERT(bool value)
{
	if (!value) {
		THROW_ERROR("");
	}
}

inline void ALWAYS_ASSERT(bool value, std::string_view errorMessage)
{
	if (!value) {
		THROW_ERROR(errorMessage);
	}
}

}
