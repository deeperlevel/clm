#pragma once

#if defined(_MSC_VER)
#include "console_msvc.hpp"

#elif defined(__GNUC__)
// Compiling with GCC or MinGW
#include "console_gcc.hpp"

#else
#error "Unsupported compiler! Only MSVC and GCC/MinGW are supported."
#endif