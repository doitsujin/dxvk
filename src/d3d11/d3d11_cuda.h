#pragma once

#include <utility>
#include <vector>

#include "../dxvk/dxvk_resource.h"

#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"

#include "d3d11_buffer.h"
#include "d3d11_texture.h"

namespace dxvk {

  class CubinShaderWrapper : public ComObject<IUnknown> {

  public:

    CubinShaderWrapper(const Rc<dxvk::DxvkDevice>& dxvkDevice, VkCuModuleNVX cuModule, VkCuFunctionNVX cuFunction, VkExtent3D blockDim);
    ~CubinShaderWrapper();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

    VkCuModuleNVX cuModule() const {
      return m_module;
    }

    VkCuFunctionNVX cuFunction() const {
      return m_function;
    }

    VkExtent3D blockDim() const {
      return m_blockDim;
    }

  private:

    Rc<DxvkDevice>  m_dxvkDevice;
    VkCuModuleNVX   m_module;
    VkCuFunctionNVX m_function;
    VkExtent3D      m_blockDim;

  };


  struct CubinShaderLaunchInfo {
    Com<CubinShaderWrapper> shader;
    std::vector<uint8_t>    params;
    size_t                  paramSize;
    VkCuLaunchInfoNVX       nvxLaunchInfo = { VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX };
    const void*             cuLaunchConfig[5];

    std::vector<std::pair<Rc<DxvkBuffer>, DxvkAccessFlags>> buffers;
    std::vector<std::pair<Rc<DxvkImage>, DxvkAccessFlags>> images;

    void insertResource(ID3D11Resource* pResource, DxvkAccessFlags access);

    template<typename T>
    static void insertUniqueResource(std::vector<std::pair<T, DxvkAccessFlags>>& list, const T& resource, DxvkAccessFlags access);
  };

}
