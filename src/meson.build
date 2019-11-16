subdir('util')
subdir('spirv')
subdir('wsi')
subdir('vulkan')
subdir('dxvk')

enable_dxgi  = get_option('enable_dxgi')
enable_d3d9  = get_option('enable_d3d9')
enable_d3d10 = get_option('enable_d3d10')
enable_d3d11 = get_option('enable_d3d11')
enable_tests = get_option('enable_tests')

if enable_d3d10 and dxvk_native
  warning('Ignoring D3D10... Not supported for DXVK native.')
  enable_d3d10 = false
endif

if enable_dxgi
  if not enable_d3d11
    error('D3D11 is required for DXGI.')
  endif
  subdir('dxgi')
endif

if enable_d3d10 or enable_d3d11 or enable_tests
  subdir('dxbc')
endif

if enable_d3d11
  subdir('d3d11')
endif

if enable_d3d10
  if not enable_d3d11
    error('D3D11 is required for D3D10.')
  endif
  subdir('d3d10')
endif

if enable_d3d9
  subdir('dxso')
  subdir('d3d9')
endif

# Nothing selected
if not enable_d3d9 and not enable_d3d10 and not enable_d3d11 and not enable_tests
  warning('Nothing selected to be built. Are you missing a frontend or tests?')
endif
