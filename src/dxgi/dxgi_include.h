#pragma once

//for some reason we need to specify __declspec(dllexport) for MinGW
#if defined(__WINE__) || !defined(_WIN32)
  #define DLLEXPORT __attribute__((visibility("default")))
#else
  #define DLLEXPORT
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
#include "../util/util_string.h"

#include <dxgi1_6.h>
