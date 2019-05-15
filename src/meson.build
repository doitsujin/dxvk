subdir('util')
subdir('spirv')
subdir('vulkan')
subdir('dxvk')

if get_option('enable_dxgi')
  if not get_option('enable_d3d10') and not get_option('enable_d3d11')
    error('D3D10 and/or D3D11 required for DXGI to properly functionning.')
  endif
  subdir('dxgi')
endif

if get_option('enable_d3d10') or get_option('enable_d3d11') or get_option('enable_tests')
  subdir('dxbc')
endif

if get_option('enable_d3d11')
  subdir('d3d11')
endif

if get_option('enable_d3d10')
  if not get_option('enable_d3d11')
    error('D3D11 required for D3D10 to properly functionning.')
  endif
  subdir('d3d10')
endif

# Nothing selected
if not get_option('enable_d3d10') and not get_option('enable_d3d11') and not get_option('enable_tests')
  error('Nothing selected to be built. Please, enable at least D3D11 or TESTS.')
endif
