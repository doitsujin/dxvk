wsi_common_src = [
  'wsi_edid.cpp',
]

wsi_win32_src = [
  'win32/wsi_monitor_win32.cpp',
  'win32/wsi_window_win32.cpp',
]

wsi_sdl2_src = [
  'sdl2/wsi_monitor_sdl2.cpp',
  'sdl2/wsi_platform_sdl2.cpp',
  'sdl2/wsi_window_sdl2.cpp',
]

wsi_glfw_src = [
  'glfw/wsi_monitor_glfw.cpp',
  'glfw/wsi_platform_glfw.cpp',
  'glfw/wsi_window_glfw.cpp',
]

if dxvk_wsi == 'win32'
  wsi_src  = wsi_common_src + wsi_win32_src
elif dxvk_wsi == 'sdl2'
  wsi_src  = wsi_common_src + wsi_sdl2_src
elif dxvk_wsi == 'glfw'
  wsi_src  = wsi_common_src + wsi_glfw_src
else
  error('Unknown wsi')
endif
wsi_deps = [ dep_displayinfo ]

wsi_lib = static_library('wsi', wsi_src,
  dependencies        : wsi_deps,
  include_directories : [ dxvk_include_path ])

wsi_dep = declare_dependency(
  link_with           : [ wsi_lib ])
