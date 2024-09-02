#include "../dxvk/dxvk_instance.h"

#include "d3d9_interface.h"
#include "d3d9_shader_validator.h"

#include "d3d9_annotation.h"

class D3DFE_PROCESSVERTICES;
using PSGPERRORID = UINT;

namespace dxvk {

  Logger Logger::s_instance("d3d9.log");
  D3D9GlobalAnnotationList D3D9GlobalAnnotationList::s_instance;

  // Vulkan 1.3 可用性检查函数
  bool checkVulkanSupport() {
    // 获取 Vulkan 支持的最高 API 版本
    uint32_t apiVersion = 0;
    VkResult versionResult = vkEnumerateInstanceVersion(&apiVersion);

    if (versionResult != VK_SUCCESS) {
		dxvk::Logger::warn("检测到你的Vulkan未安装.");
      return false;
    }

    // 检查是否支持 Vulkan 1.3
    if (VK_VERSION_MAJOR(apiVersion) < 1 || VK_VERSION_MINOR(apiVersion) < 3) {
		dxvk::Logger::warn("需要Vulkan 1.3版本,你的Vulkan版本是："+std::to_string(VK_VERSION_MAJOR(apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(apiVersion)));
      return false;
    }

    Logger::s_instance.info("Vulkan 1.3 支持.");
    return true;
  }

  HRESULT CreateD3D9(
          bool           Extended,
          IDirect3D9Ex** ppDirect3D9Ex) {
    
    if (!ppDirect3D9Ex)
      return D3DERR_INVALIDCALL;

    // 检查 Vulkan 1.3 可用性
    if (!checkVulkanSupport()) {
      return D3DERR_NOTAVAILABLE;
    }

    *ppDirect3D9Ex = ref(new D3D9InterfaceEx(Extended));
    return D3D_OK;
  }
}

extern "C" {

  DLLEXPORT IDirect3D9* __stdcall Direct3DCreate9(UINT nSDKVersion) {
    IDirect3D9Ex* pDirect3D = nullptr;
    dxvk::CreateD3D9(false, &pDirect3D);

    return pDirect3D;
  }

  DLLEXPORT HRESULT __stdcall Direct3DCreate9Ex(UINT nSDKVersion, IDirect3D9Ex** ppDirect3D9Ex) {
    return dxvk::CreateD3D9(true, ppDirect3D9Ex);
  }

  DLLEXPORT int __stdcall D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) {
    return dxvk::D3D9GlobalAnnotationList::Instance().BeginEvent(col, wszName);
  }

  DLLEXPORT int __stdcall D3DPERF_EndEvent(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().EndEvent();
  }

  DLLEXPORT void __stdcall D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetMarker(col, wszName);
  }

  DLLEXPORT void __stdcall D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetRegion(col, wszName);
  }

  DLLEXPORT BOOL __stdcall D3DPERF_QueryRepeatFrame(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().QueryRepeatFrame();
  }

  DLLEXPORT void __stdcall D3DPERF_SetOptions(DWORD dwOptions) {
    dxvk::D3D9GlobalAnnotationList::Instance().SetOptions(dwOptions);
  }

  DLLEXPORT DWORD __stdcall D3DPERF_GetStatus(void) {
    return dxvk::D3D9GlobalAnnotationList::Instance().GetStatus();
  }


  DLLEXPORT void __stdcall DebugSetMute(void) {
  }

  DLLEXPORT int __stdcall DebugSetLevel(void) {
    return 0;
  }

  // Processor Specific Geometry Pipeline
  // for P3 SIMD/AMD 3DNow.

  DLLEXPORT void __stdcall PSGPError(D3DFE_PROCESSVERTICES* a, PSGPERRORID b, UINT c) {
  }

  DLLEXPORT void __stdcall PSGPSampleTexture(D3DFE_PROCESSVERTICES* a, UINT b, float(*const c)[4], UINT d, float(*const e)[4]) {
  }

  DLLEXPORT dxvk::D3D9ShaderValidator* __stdcall Direct3DShaderValidatorCreate9(void) {
    return ref(new dxvk::D3D9ShaderValidator());
  }

  DLLEXPORT int __stdcall Direct3D9EnableMaximizedWindowedModeShim(UINT a) {
    return 0;
  }

  DLLEXPORT void __stdcall DXVK_RegisterAnnotation(IDXVKUserDefinedAnnotation* annotation) {
    dxvk::D3D9GlobalAnnotationList::Instance().RegisterAnnotator(annotation);
  }

  DLLEXPORT void __stdcall DXVK_UnRegisterAnnotation(IDXVKUserDefinedAnnotation* annotation) {
    dxvk::D3D9GlobalAnnotationList::Instance().UnregisterAnnotator(annotation);
  }

  DLLEXPORT void __stdcall Direct3D9ForceHybridEnumeration(UINT uHybrid) {
  }

  DLLEXPORT IDirect3D9* __stdcall Direct3DCreate9On12(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count) {
    dxvk::Logger::warn("Direct3DCreate9On12: 9On12 functionality is unimplemented.");
    return Direct3DCreate9(sdk_version);
  }

  DLLEXPORT HRESULT __stdcall Direct3DCreate9On12Ex(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count, IDirect3D9Ex** output) {
    dxvk::Logger::warn("Direct3DCreate9On12Ex: 9On12 functionality is unimplemented.");
    return Direct3DCreate9Ex(sdk_version, output);
  }

}
