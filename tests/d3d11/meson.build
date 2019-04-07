test_d3d11_opts = common_exe_opts + { 'dependencies': [ util_dep, lib_dxgi, lib_d3d11, lib_d3dcompiler_47 ] }

executable('d3d11-compute'+exe_ext,   files('test_d3d11_compute.cpp'),   kwargs: test_d3d11_opts)
executable('d3d11-formats'+exe_ext,   files('test_d3d11_formats.cpp'),   kwargs: test_d3d11_opts)
executable('d3d11-map-read'+exe_ext,  files('test_d3d11_map_read.cpp'),  kwargs: test_d3d11_opts)
executable('d3d11-streamout'+exe_ext, files('test_d3d11_streamout.cpp'), kwargs: test_d3d11_opts)
executable('d3d11-triangle'+exe_ext,  files('test_d3d11_triangle.cpp'),  kwargs: test_d3d11_opts)
