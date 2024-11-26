#pragma once

#ifdef DXVK_WSI_WIN32
#error You shouldnt be using this code path.
#endif

#ifdef DXVK_WSI_SDL3
#include "wsi/native_sdl3.h"
#endif

#ifdef DXVK_WSI_SDL2
#include "wsi/native_sdl2.h"
#endif

#ifdef DXVK_WSI_GLFW
#include "wsi/native_glfw.h"
#endif

#if !definedDXVK_WSI_SDL3) && !defined(DXVK_WSI_SDL2) && !defined(DXVK_WSI_GLFW)
#error Unknown wsi!
#endif
