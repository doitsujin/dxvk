#pragma once

#if defined(DXVK_WSI_WIN32)
#include "win32/wsi_platform_win32.h"
#elif defined(DXVK_WSI_SDL2)
#include "sdl2/wsi_platform_sdl2.h"
#elif defined(DXVK_WSI_GLFW)
#include "glfw/wsi_platform_glfw.h"
#elif defined(DXVK_WSI_NONE)
#include "none/wsi_platform_none.h"
#endif
