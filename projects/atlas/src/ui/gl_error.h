#pragma once
#include <string_view>

void _check_gl_error(std::string_view file, int line);

#ifdef DNDEBUG
#define check_gl_error()
#else
#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
#endif