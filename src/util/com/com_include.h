#pragma once

// GCC complains about the COM interfaces
// not having virtual destructors
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif // __GNUC__

#include "../util_targetver.h"
#include <windows.h>
#include <unknwn.h>

// GCC: -std options disable certain keywords
// https://gcc.gnu.org/onlinedocs/gcc/Alternate-Keywords.html
#if defined(__WINE__) && !defined(typeof)
#define typeof __typeof
#endif
