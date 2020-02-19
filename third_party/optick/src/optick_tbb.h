#pragma once
#include "optick.h"

namespace Optick {

#if USE_OPTICK
OPTICK_API void tryRegisterThreadWithOptick();
OPTICK_API void setThisMainThreadOptick();
#else
inline void tryRegisterThreadWithOptick() {}
inline void setThisMainThreadOptick() {}
#endif
}