#pragma once

// GCC complains about the COM interfaces
// not having virtual destructors
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif // __GNUC__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>