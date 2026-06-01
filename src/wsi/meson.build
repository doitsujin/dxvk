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
  find = find_program('find')

  if platform == 'darwin'
    wsi_soname_conf.set('WSI_SDL2_SONAME', 'libSDL2-2.0.0.dylib')
    wsi_soname_conf.set('WSI_SDL3_SONAME', 'libSDL3.0.dylib')
    wsi_soname_conf.set('WSI_GLFW_SONAME', 'libglfw.3.dylib')
  else
    wsi_soname_conf.set('WSI_SDL2_SONAME', 'libSDL2-2.0.so.0')
    wsi_soname_conf.set('WSI_SDL3_SONAME', 'libSDL3.so.0')
    wsi_soname_conf.set('WSI_GLFW_SONAME', 'libglfw.so.3')
  endif

  if lib_sdl2.found()
    sdl2_libdir = lib_sdl2.get_variable(pkgconfig: 'libdir', default_value: '')
    if sdl2_libdir != ''
      if platform == 'darwin'
        sdl2_glob = 'libSDL2*.dylib'
      else
        sdl2_glob = 'libSDL2*.so*'
      endif
      sdl2_find = run_command(find, sdl2_libdir, '-maxdepth', '1', '-name', sdl2_glob, check: false)
      if sdl2_find.returncode() == 0
        foreach sdl2_lib : sdl2_find.stdout().strip().split('\n')
          if sdl2_lib != ''
            sdl2_name = fs.name(sdl2_lib)
            if platform == 'darwin'
              wsi_soname_conf.set('WSI_SDL2_SONAME', sdl2_name)
              break
            elif sdl2_name.endswith('.so.0') and not sdl2_name.contains('.so.0.')
              wsi_soname_conf.set('WSI_SDL2_SONAME', sdl2_name)
              break
            endif
          endif
        endforeach
      endif
    endif
  endif

  if lib_sdl3.found()
    sdl3_libdir = lib_sdl3.get_variable(pkgconfig: 'libdir', default_value: '')
    if sdl3_libdir != ''
      if platform == 'darwin'
        sdl3_glob = 'libSDL3*.dylib'
      else
        sdl3_glob = 'libSDL3*.so*'
      endif
      sdl3_find = run_command(find, sdl3_libdir, '-maxdepth', '1', '-name', sdl3_glob, check: false)
      if sdl3_find.returncode() == 0
        foreach sdl3_lib : sdl3_find.stdout().strip().split('\n')
          if sdl3_lib != ''
            sdl3_name = fs.name(sdl3_lib)
            if platform == 'darwin'
              wsi_soname_conf.set('WSI_SDL3_SONAME', sdl3_name)
              break
            elif sdl3_name.endswith('.so.0') and not sdl3_name.contains('.so.0.')
              wsi_soname_conf.set('WSI_SDL3_SONAME', sdl3_name)
              break
            endif
          endif
        endforeach
      endif
    endif
  endif

  if lib_glfw.found()
    glfw_libdir = lib_glfw.get_variable(pkgconfig: 'libdir', default_value: '')
    if glfw_libdir != ''
      if platform == 'darwin'
        glfw_glob = 'libglfw*.dylib'
      else
        glfw_glob = 'libglfw*.so*'
      endif
      glfw_find = run_command(find, glfw_libdir, '-maxdepth', '1', '-name', glfw_glob, check: false)
      if glfw_find.returncode() == 0
        foreach glfw_lib : glfw_find.stdout().strip().split('\n')
          if glfw_lib != ''
            glfw_name = fs.name(glfw_lib)
            if platform == 'darwin'
              wsi_soname_conf.set('WSI_GLFW_SONAME', glfw_name)
              break
            elif glfw_name.endswith('.so.3') and not glfw_name.contains('.so.3.')
              wsi_soname_conf.set('WSI_GLFW_SONAME', glfw_name)
              break
            endif
          endif
        endforeach
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
