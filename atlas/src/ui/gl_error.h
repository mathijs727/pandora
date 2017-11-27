#pragma once
#include <gsl/gsl>

void _check_gl_error(gsl::cstring_span<> file, int line);

#ifdef DNDEBUG
#define check_gl_error()
#else
#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
#endif