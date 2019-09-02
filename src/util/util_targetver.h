#pragma once

// We want to target Windows 10,
// so we need to define this before
// including the following header...

#define WINVER       0x0A00
#define _WIN32_WINNT 0x0A00

// Exclude some superflous stuff...
#define WIN32_LEAN_AND_MEAN

#include <sdkddkver.h>