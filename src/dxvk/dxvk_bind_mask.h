#pragma once

#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_image.h"
#include "dxvk_limits.h"
#include "dxvk_sampler.h"

namespace dxvk {
  
  /**
   * \brief Binding mask
   * 
   * Used to track which resource slots have a compatible
   * binding and which ones don't. This is used to set up
   * binding-related specialization constants in shaders.
   * \tparam N Number of binding slots
   */
  template<uint32_t BindingCount>
  class DxvkBindingSet {
    constexpr static uint32_t BitCount = 32;
    constexpr static uint32_t IntCount = (BindingCount + BitCount - 1) / BitCount;
  public:
    
    /**
     * \brief Tests whether a binding is active
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the binding is active
     */
    bool test(uint32_t slot) const {
      const uint32_t intId = slot / BitCount;
      const uint32_t bitId = slot % BitCount;
      const uint32_t bitMask = 1u << bitId;
      return (m_slots[intId] & bitMask) != 0;
    }
    
    /**
     * \brief Changes a single binding
     * 
     * \param [in] slot The binding ID
     * \param [in] value New binding state
     * \returns \c true if the state has changed
     */
    bool set(uint32_t slot, bool value) {
      const uint32_t intId = slot / BitCount;
      const uint32_t bitId = slot % BitCount;
      const uint32_t bitMask = 1u << bitId;
      
      const uint32_t prev = m_slots[intId];
      const uint32_t next = value
        ? prev |  bitMask
        : prev & ~bitMask;
      m_slots[intId] = next;
      return prev != next;
    }

    /**
     * \brief Marks a binding as active
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the state has changed
     */
    bool set(uint32_t slot) {
      return set(slot, true);
    }

    /**
     * \brief Marks a binding as inactive
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the state has changed
     */
    bool clr(uint32_t slot) {
      return set(slot, false);
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

    /**
     * \brief Enables multiple bindings
     * \param [in] n Number of bindings
     */
    void setFirst(uint32_t n) {
      for (uint32_t i = 0; i < IntCount; i++) {
        m_slots[i] = n >= BitCount ? ~0u : ~(~0u << n);
        n = n >= BitCount ? n - BitCount : 0;
      }
    }

    /**
     * \brief Finds next set binding
     *
     * \param [in] first Fist bit to consider
     * \returns Binding ID, or -1 if none was found
     */
    int32_t findNext(uint32_t first) const {
      if (unlikely(first >= BindingCount))
        return -1;

      uint32_t intId = first / BitCount;
      uint32_t bitId = first % BitCount;

      auto mask = m_slots[intId] & ~((1 << bitId) - 1);

      while (!mask && ++intId < IntCount)
        mask = m_slots[intId];
      
      if (!mask)
        return -1;
      
      return BitCount * intId + bit::tzcnt(mask);
    }

    bool operator == (const DxvkBindingSet& other) const {
      bool eq = true;
      for (uint32_t i = 0; i < IntCount; i++)
        eq &= m_slots[i] == other.m_slots[i];
      return eq;
    }

    bool operator != (const DxvkBindingSet& other) const {
      return !this->operator == (other);
    }
    
  private:
    
    uint32_t m_slots[IntCount];
    
  };

  using DxvkBindingMask = DxvkBindingSet<MaxNumActiveBindings>;
  
  
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