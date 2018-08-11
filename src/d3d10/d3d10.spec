@ stdcall D3D10CreateDevice(ptr long ptr long long ptr)
@ stdcall D3D10CreateDeviceAndSwapChain(ptr long ptr long long ptr ptr ptr)

@ stdcall D3D10GetVertexShaderProfile(ptr)
@ stdcall D3D10GetGeometryShaderProfile(ptr)
@ stdcall D3D10GetPixelShaderProfile(ptr)

@ stdcall D3D10CreateBlob(long ptr)
@ stdcall D3D10GetInputSignatureBlob(ptr long ptr)
@ stdcall D3D10GetOutputSignatureBlob(ptr long ptr)

@ stdcall D3D10ReflectShader(ptr long ptr)
@ stdcall D3D10CompileShader(ptr long str ptr ptr str str long ptr ptr)

@ stdcall D3D10CreateEffectFromMemory(ptr long long ptr ptr ptr)
@ stdcall D3D10CreateEffectPoolFromMemory(ptr long long ptr ptr)
@ stdcall D3D10CompileEffectFromMemory(ptr long ptr ptr ptr long long ptr ptr)

@ stub D3D10DisassembleEffect
@ stdcall D3D10DisassembleShader(ptr long long ptr ptr)
@ stub D3D10PreprocessShader
