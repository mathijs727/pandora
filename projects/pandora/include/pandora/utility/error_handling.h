#pragma once
#include <iostream>
#include <string_view>

namespace pandora
{

inline void THROW_ERROR(std::string_view errorMessage)
{
	std::cerr << errorMessage << std::endl;
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

}
