wsi_src = [
  'wsi_edid.cpp',
  'wsi_platform.cpp',
  'win32/wsi_monitor_win32.cpp',
  'win32/wsi_platform_win32.cpp',
  'win32/wsi_window_win32.cpp',
  'sdl3/wsi_monitor_sdl3.cpp',
  'sdl3/wsi_platform_sdl3.cpp',
  'sdl3/wsi_window_sdl3.cpp',
  'sdl2/wsi_monitor_sdl2.cpp',
  'sdl2/wsi_platform_sdl2.cpp',
  'sdl2/wsi_window_sdl2.cpp',
  'glfw/wsi_monitor_glfw.cpp',
  'glfw/wsi_platform_glfw.cpp',
  'glfw/wsi_window_glfw.cpp',
]

wsi_deps = [ dep_displayinfo ]

wsi_soname_conf = configuration_data()
if platform == 'windows'
  wsi_soname_conf.set('WSI_SDL2_SONAME', 'SDL2.dll')
  wsi_soname_conf.set('WSI_SDL3_SONAME', 'SDL3.dll')
  wsi_soname_conf.set('WSI_GLFW_SONAME', 'glfw.dll')
else
  fs = import('fs')

  wsi_soname_conf.set('WSI_SDL2_SONAME', 'libSDL2-2.0.0.dylib' if platform == 'darwin' else 'libSDL2-2.0.so.0')
  wsi_soname_conf.set('WSI_SDL3_SONAME', 'libSDL3.0.dylib' if platform == 'darwin' else 'libSDL3.so.0')
  wsi_soname_conf.set('WSI_GLFW_SONAME', 'libglfw.3.dylib' if platform == 'darwin' else 'libglfw.so.3')

  if lib_sdl2.found()
    sdl2_libdir = lib_sdl2.get_variable(pkgconfig: 'libdir', default_value: '')
    if sdl2_libdir != ''
      sdl2_lib = cpp.find_library('SDL2', dirs: sdl2_libdir, required: false)
      if sdl2_lib.found()
        wsi_soname_conf.set('WSI_SDL2_SONAME', fs.name(sdl2_lib.full_path()))
      endif
    endif
  endif

  if lib_sdl3.found()
    sdl3_libdir = lib_sdl3.get_variable(pkgconfig: 'libdir', default_value: '')
    if sdl3_libdir != ''
      sdl3_lib = cpp.find_library('SDL3', dirs: sdl3_libdir, required: false)
      if sdl3_lib.found()
        wsi_soname_conf.set('WSI_SDL3_SONAME', fs.name(sdl3_lib.full_path()))
      endif
    endif
  endif

  if lib_glfw.found()
    glfw_libdir = lib_glfw.get_variable(pkgconfig: 'libdir', default_value: '')
    if glfw_libdir != ''
      glfw_lib = cpp.find_library('glfw', dirs: glfw_libdir, required: false)
      if glfw_lib.found()
        wsi_soname_conf.set('WSI_GLFW_SONAME', fs.name(glfw_lib.full_path()))
      endif
    endif
  endif
endif

wsi_sonames = configure_file(
  configuration : wsi_soname_conf,
  input         : 'wsi_sonames.h.in',
  output        : 'wsi_sonames.h',
)

wsi_include_dirs = [
  dxvk_include_path,
  include_directories('.'),
]

if platform == 'darwin'
  wsi_src += 'darwin/wsi_edid_darwin.mm'
  wsi_deps += dependency('appleframeworks', modules : [ 'CoreGraphics', 'Foundation', 'IOKit' ])
endif

if platform == 'windows'
  lib_setupapi = cpp.find_library('setupapi')
  wsi_deps += [ lib_setupapi ]
else
  wsi_deps += [
    lib_sdl3.partial_dependency(compile_args: true, includes: true),
    lib_sdl2.partial_dependency(compile_args: true, includes: true),
    lib_glfw.partial_dependency(compile_args: true, includes: true),
  ]
endif

wsi_lib = static_library('wsi', wsi_src,
  dependencies        : wsi_deps,
  include_directories : wsi_include_dirs)

wsi_dep = declare_dependency(
  link_with           : [ wsi_lib ])
