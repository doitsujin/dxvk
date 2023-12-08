wsi_src = [
  'wsi_edid.cpp',
  'wsi_platform.cpp',
  'win32/wsi_monitor_win32.cpp',
  'win32/wsi_platform_win32.cpp',
  'win32/wsi_window_win32.cpp',
  'sdl2/wsi_monitor_sdl2.cpp',
  'sdl2/wsi_platform_sdl2.cpp',
  'sdl2/wsi_window_sdl2.cpp',
  'glfw/wsi_monitor_glfw.cpp',
  'glfw/wsi_platform_glfw.cpp',
  'glfw/wsi_window_glfw.cpp',
]

wsi_deps = [ dep_displayinfo ]

wsi_lib = static_library('wsi', wsi_src,
  dependencies        : wsi_deps,
  include_directories : [ dxvk_include_path ])

wsi_dep = declare_dependency(
  link_with           : [ wsi_lib ])
