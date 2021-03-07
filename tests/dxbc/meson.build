test_dxbc_opts     = common_exe_opts + { 'dependencies': [ dxbc_dep, dxvk_dep ] }
test_dxbc_ext_opts = common_exe_opts + { 'dependencies': [ dxbc_dep, dxvk_dep, lib_d3dcompiler_47 ] }

executable('dxbc-compiler'+exe_ext, files('test_dxbc_compiler.cpp'), kwargs: test_dxbc_opts)
executable('dxbc-disasm'+exe_ext,   files('test_dxbc_disasm.cpp'),   kwargs: test_dxbc_ext_opts)
executable('hlsl-compiler'+exe_ext, files('test_hlsl_compiler.cpp'), kwargs: test_dxbc_ext_opts)
