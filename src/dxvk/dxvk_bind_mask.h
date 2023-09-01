#pragma once

#include <type_traits>

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
    using MaskType = std::conditional_t<(BindingCount > 32), uintptr_t, uint32_t>;

    constexpr static MaskType SetBit = MaskType(1u);
    constexpr static MaskType SetMask = ~MaskType(0u);

    constexpr static uint32_t BitCount = 8 * sizeof(MaskType);
    constexpr static uint32_t IntCount = (BindingCount + BitCount - 1) / BitCount;
  public:
    
    /**
     * \brief Tests whether a binding is active
     * 
     * \param [in] slot The binding ID
     * \returns \c true if the binding is active
     */
    bool test(uint32_t slot) const {
      const uint32_t intId = computeIntId(slot);
      const uint32_t bitId = computeBitId(slot);
      const MaskType bitMask = SetBit << bitId;
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
      const uint32_t intId = computeIntId(slot);
      const uint32_t bitId = computeBitId(slot);
      const MaskType bitMask = SetBit << bitId;
      
      const MaskType prev = m_slots[intId];
      const MaskType next = value
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
     *
     * Leaves bindings outside of this range unaffected.
     * \param [in] first First binding to enable
     * \param [in] count Number of bindings to enable
     */
    void setRange(uint32_t first, uint32_t count) {
      if (!count)
        return;

      uint32_t firstInt = computeIntId(first);
      uint32_t firstBit = computeBitId(first);

      uint32_t lastInt = computeIntId(first + count - 1);
      uint32_t lastBit = computeBitId(first + count - 1) + 1;

      if (firstInt == lastInt) {
        m_slots[firstInt] |= (count < BitCount)
          ? ((SetBit << count) - 1) << firstBit
          : (SetMask);
      } else {
        m_slots[firstInt] |= SetMask << firstBit;
        m_slots[lastInt] |= SetMask >> (BitCount - lastBit);

        for (uint32_t i = firstInt + 1; i < lastInt; i++)
          m_slots[i] = SetMask;
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

      uint32_t intId = computeIntId(first);
      uint32_t bitId = computeBitId(first);

      MaskType mask = m_slots[intId] & ~((SetBit << bitId) - 1);

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
    
    MaskType m_slots[IntCount];

    static uint32_t computeIntId(uint32_t slot) {
      if constexpr (IntCount > 1)
        return slot / BitCount;
      else
        return 0;
    }

    static uint32_t computeBitId(uint32_t slot) {
      if constexpr (IntCount > 1)
        return slot % BitCount;
      else
        return slot;
    }
    
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