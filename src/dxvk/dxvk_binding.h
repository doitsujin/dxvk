#pragma once

#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_image.h"
#include "dxvk_limits.h"
#include "dxvk_sampler.h"

namespace dxvk {
  
  /**
   * \brief Binding state
   * 
   * Used to track which resource slots have a compatible
   * binding and which ones don't. This is used to set up
   * binding-related specialization constants in shaders.
   * \tparam N Number of binding slots
   */
  class DxvkBindingState {
    constexpr static uint32_t BitCount = 32;
    constexpr static uint32_t IntCount = (MaxNumActiveBindings + BitCount - 1) / BitCount;
  public:
    
    /**
     * \brief Tests whether a binding is active
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the binding is active
     */
    bool isBound(uint32_t slot) const {
      const uint32_t intId = slot / BitCount;
      const uint32_t bitId = slot % BitCount;
      const uint32_t bitMask = 1u << bitId;
      return (m_slots[intId] & bitMask) != 0;
    }
    
    /**
     * \brief Marks a binding as active
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the state has changed
     */
    bool setBound(uint32_t slot) {
      const uint32_t intId = slot / BitCount;
      const uint32_t bitId = slot % BitCount;
      const uint32_t bitMask = 1u << bitId;
      
      const uint32_t prev = m_slots[intId];
      m_slots[intId] = prev | bitMask;
      return (prev & bitMask) == 0;
    }
    
    /**
     * \brief Marks a binding as inactive
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the state has changed
     */
    bool setUnbound(uint32_t slot) {
      const uint32_t intId = slot / BitCount;
      const uint32_t bitId = slot % BitCount;
      const uint32_t bitMask = 1u << bitId;
      
      const uint32_t prev = m_slots[intId];
      m_slots[intId] = prev & ~bitMask;
      return (prev & bitMask) != 0;
    }
    
    /**
     * \brief Clears binding state
     * 
     * Useful to zero out any bindings
     * that are not used by a pipeline.
     */
    void clear() {
      for (uint32_t i = 0; i < IntCount; i++)
        m_slots[i] = 0;
    }
    
  private:
    
    uint32_t m_slots[IntCount];
    
  };
  
  
  /**
   * \brief Bound shader resources
   * 
   * Stores the resources bound to a binding
   * slot in DXVK. These are used to create
   * descriptor sets.
   */
  struct DxvkShaderResourceSlot {
    Rc<DxvkSampler>    sampler;
    Rc<DxvkImageView>  imageView;
    Rc<DxvkBufferView> bufferView;
    DxvkBufferSlice    bufferSlice;
  };
  
}