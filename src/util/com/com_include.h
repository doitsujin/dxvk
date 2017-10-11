#pragma once

// GCC complains about the COM interfaces
// not having virtual destructors
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>