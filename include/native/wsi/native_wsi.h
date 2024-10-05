#pragma once

#ifdef DXVK_WSI_WIN32
#error You shouldnt be using this code path.
#elif DXVK_WSI_SDL3
#include "wsi/native_sdl3.h"
#elif DXVK_WSI_SDL2
#include "wsi/native_sdl2.h"
#elif DXVK_WSI_GLFW
#include "wsi/native_glfw.h"
#else
#error Unknown wsi!
#endif