subdir('wsi')
subdir('util')
subdir('spirv')
subdir('vulkan')
subdir('dxvk')

if get_option('enable_dxgi')
  subdir('dxgi')
endif

if get_option('enable_d3d10') or get_option('enable_d3d11')
  subdir('dxbc')
endif

if get_option('enable_d3d11')
  if not get_option('enable_dxgi')
    error('DXGI is required for D3D11.')
  endif
  subdir('d3d11')
endif

if get_option('enable_d3d10')
  if not get_option('enable_d3d11')
    error('D3D11 is required for D3D10.')
  endif
  subdir('d3d10')
endif

if get_option('enable_d3d9')
  subdir('dxso')
  subdir('d3d9')
endif

# Nothing selected
if not get_option('enable_d3d9') and not get_option('enable_dxgi')
  warning('Nothing selected to be built.?')
endif
