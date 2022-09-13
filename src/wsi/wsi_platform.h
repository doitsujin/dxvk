#pragma once

#if defined(DXVK_WSI_WIN32)
#include "win32/wsi_platform_win32.h"
#elif defined(DXVK_WSI_SDL2)
#include "sdl2/wsi_platform_sdl2.h"
#endif
