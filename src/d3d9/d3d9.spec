@ stdcall Direct3DCreate9(long)

@ stdcall Direct3DCreate9Ex(long ptr)

@ stdcall D3DPERF_BeginEvent(long wstr)
@ stdcall D3DPERF_EndEvent()
@ stdcall D3DPERF_SetMarker(long wstr)
@ stdcall D3DPERF_SetRegion(long wstr)
@ stdcall D3DPERF_QueryRepeatFrame()
@ stdcall D3DPERF_SetOptions(long)
@ stdcall D3DPERF_GetStatus()

@ stdcall DebugSetMute()
@ stdcall DebugSetLevel()
@ stdcall PSGPError(ptr long long)
@ stdcall PSGPSampleTexture(ptr long ptr long ptr)
@ stdcall Direct3DShaderValidatorCreate9()
@ stdcall Direct3D9EnableMaximizedWindowedModeShim(long)