#pragma once

#include <memory>
#include <unordered_map>

#include "d3d11_include.h"

namespace dxvk {
  
  class D3D11Device;
  
  struct D3D11StateDescHash {
    size_t operator () (const D3D11_BLEND_DESC1& desc) const;
    size_t operator () (const D3D11_DEPTH_STENCILOP_DESC& desc) const;
    size_t operator () (const D3D11_DEPTH_STENCIL_DESC& desc) const;
    size_t operator () (const D3D11_RASTERIZER_DESC2& desc) const;
    size_t operator () (const D3D11_RENDER_TARGET_BLEND_DESC1& desc) const;
    size_t operator () (const D3D11_SAMPLER_DESC& desc) const;
  };
  
  
  struct D3D11StateDescEqual {
    bool operator () (const D3D11_BLEND_DESC1& a, const D3D11_BLEND_DESC1& b) const;
    bool operator () (const D3D11_DEPTH_STENCILOP_DESC& a, const D3D11_DEPTH_STENCILOP_DESC& b) const;
    bool operator () (const D3D11_DEPTH_STENCIL_DESC& a, const D3D11_DEPTH_STENCIL_DESC& b) const;
    bool operator () (const D3D11_RASTERIZER_DESC2& a, const D3D11_RASTERIZER_DESC2& b) const;
    bool operator () (const D3D11_RENDER_TARGET_BLEND_DESC1& a, const D3D11_RENDER_TARGET_BLEND_DESC1& b) const;
    bool operator () (const D3D11_SAMPLER_DESC& a, const D3D11_SAMPLER_DESC& b) const;
  };
  
  
  /**
   * \brief Unique state object set
   * 
   * When creating state objects, D3D11 first checks if
   * an object with the same description already exists
   * and returns it if that is the case. This class
   * implements that behaviour.
   */
  template<typename T>
  class D3D11StateObjectSet {
    using DescType = typename T::DescType;
  public:
    
    /**
     * \brief Retrieves a state object
     * 
     * Returns an object with the same description or
     * creates a new one if no such object exists.
     * \param [in] device The calling D3D11 device
     * \param [in] desc State object description
     * \returns Pointer to the state object
     */
    T* Create(D3D11Device* device, const DescType& desc) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      auto entry = m_objects.find(desc);

      if (entry != m_objects.end())
        return ref(entry->second.get());

      auto result = m_objects.insert({ desc,
        std::make_unique<T>(device, desc, this) });
      return ref(result.first->second.get());
    }

    /**
     * \brief Destroys a state object
     *
     * If the object is no longer in use, it will be
     * removed from the look-up table.
     * \param [in] object Pointer to object to destroy
     */
    void Destroy(T* object, uint32_t version) {
      std::unique_ptr<T> allocation;

      // This code can re-enter via destruction notifier callbacks,
      // Make sure that we keep the look-up table in a valid state
      // at the time the actual state object gets destroyed.
      { std::lock_guard<dxvk::mutex> lock(m_mutex);

        if (object->IsCurrent(version)) {
          auto entry = m_objects.find(object->Desc());

          if (entry != m_objects.end()) {
            allocation = std::move(entry->second);
            m_objects.erase(entry);
          }
        }
      }
    }

  private:
    
    dxvk::mutex                                 m_mutex;
    std::unordered_map<DescType, std::unique_ptr<T>,
      D3D11StateDescHash, D3D11StateDescEqual>  m_objects;
    
  };
  
}
