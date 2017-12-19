#pragma once

#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_image.h"
#include "dxvk_sampler.h"

namespace dxvk {
  
  /**
   * \brief Bound shader resource
   */
  struct DxvkShaderResourceSlot {
    Rc<DxvkSampler>    sampler;
    Rc<DxvkImageView>  imageView;
    Rc<DxvkBufferView> bufferView;
    DxvkBufferSlice    bufferSlice;
  };
  
  
  /**
   * \brief Shader resource slots
   */
  class DxvkShaderResourceSlots {
    
  public:
    
    DxvkShaderResourceSlots() { }
    DxvkShaderResourceSlots(size_t n) {
      m_resources.resize(n);
    }
    
    const DxvkShaderResourceSlot& getShaderResource(uint32_t slot) const {
      return m_resources.at(slot);
    }
    
    void bindShaderResource(
            uint32_t                slot,
      const DxvkShaderResourceSlot& resource) {
      m_resources.at(slot) = resource;
    }
    
  private:
    
    std::vector<DxvkShaderResourceSlot> m_resources;
    
  };
  
}