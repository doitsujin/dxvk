#include "d3d11_cuda.h"

namespace dxvk {

  CubinShaderWrapper::CubinShaderWrapper(const Rc<dxvk::DxvkDevice>& dxvkDevice, VkCuModuleNVX cuModule, VkCuFunctionNVX cuFunction, VkExtent3D blockDim)
  : m_dxvkDevice(dxvkDevice), m_module(cuModule), m_function(cuFunction), m_blockDim(blockDim) { };


  CubinShaderWrapper::~CubinShaderWrapper() {
    VkDevice vkDevice = m_dxvkDevice->handle();
    m_dxvkDevice->vkd()->vkDestroyCuFunctionNVX(vkDevice, m_function, nullptr);
    m_dxvkDevice->vkd()->vkDestroyCuModuleNVX(vkDevice, m_module, nullptr);
  };


  HRESULT STDMETHODCALLTYPE CubinShaderWrapper::QueryInterface(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(IUnknown)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("CubinShaderWrapper::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  void CubinShaderLaunchInfo::insertResource(ID3D11Resource* pResource, DxvkAccessFlags access) {
    auto img = GetCommonTexture(pResource);
    auto buf = GetCommonBuffer(pResource);

    if (img)
      insertUniqueResource(images, img->GetImage(), access);
    if (buf)
      insertUniqueResource(buffers, buf->GetBuffer(), access);
  }


  template<typename T>
  void CubinShaderLaunchInfo::insertUniqueResource(std::vector<std::pair<T, DxvkAccessFlags>>& list, const T& resource, DxvkAccessFlags access) {
    for (auto& entry : list) {
      if (entry.first == resource) {
        entry.second.set(access);
        return;
      }
    }

    list.push_back({ resource, access });
  }

}
