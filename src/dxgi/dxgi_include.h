#pragma once

//for some reason we need to specify __declspec(dllexport) for MinGW
#if defined(__WINE__)
  #define DLLEXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
  #define DLLEXPORT
#else
  #define DLLEXPORT __declspec(dllexport)
#endif

#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"
#include "../util/com/com_pointer.h"

#include "../util/log/log.h"
#include "../util/log/log_debug.h"

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#include "../util/util_env.h"
#include "../util/util_enum.h"
#include "../util/util_error.h"
#include "../util/util_flags.h"
#include "../util/util_likely.h"
#include "../util/util_math.h"
#include "../util/util_monitor.h"
#include "../util/util_string.h"

#include <dxgi1_6.h>

// For some reason, these are not exposed
#ifndef DXGI_RESOURCE_PRIORITY_NORMAL
  #define DXGI_RESOURCE_PRIORITY_MINIMUM (0x28000000)
  #define DXGI_RESOURCE_PRIORITY_LOW (0x50000000)
  #define DXGI_RESOURCE_PRIORITY_NORMAL (0x78000000)
  #define DXGI_RESOURCE_PRIORITY_HIGH (0xa0000000)
  #define DXGI_RESOURCE_PRIORITY_MAXIMUM (0xc8000000)
#endif

#ifndef DXGI_CPU_ACCESS_NONE
  #define DXGI_CPU_ACCESS_NONE       (0)
  #define DXGI_CPU_ACCESS_DYNAMIC    (1)
  #define DXGI_CPU_ACCESS_READ_WRITE (2)
  #define DXGI_CPU_ACCESS_SCRATCH    (3)
  #define DXGI_CPU_ACCESS_FIELD      (0xf)
#endif