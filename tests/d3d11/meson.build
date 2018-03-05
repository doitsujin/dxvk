test_d3d11_deps = [ util_dep, lib_dxgi, lib_d3d11, lib_d3dcompiler_47 ]

executable('d3d11-compute',  files('test_d3d11_compute.cpp'),  dependencies : test_d3d11_deps, install : true, override_options: ['cpp_std='+dxvk_cpp_std])
executable('d3d11-triangle', files('test_d3d11_triangle.cpp'), dependencies : test_d3d11_deps, install : true, override_options: ['cpp_std='+dxvk_cpp_std])