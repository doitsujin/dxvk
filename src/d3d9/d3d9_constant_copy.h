#pragma once

#include <optional>

#include "../util/util_small_vector.h"
#include "../util/util_vector.h"

namespace dxvk {

  /**
   * \brief Constant type
   */
  enum class D3D9ConstantType {
    Float,
    Int,
    Bool
  };

  /**
   * \brief Constant range
   *
   * Used to set up the exact layout of a constant buffer.
   * Constant indices for booleans must actually be given
   * in units of dwords rather than bits.
   */
  struct D3D9ConstantRange {
    /** Constant index in buffer */
    uint16_t dstIndex = 0u;
    /** Constant index in API state */
    uint16_t srcIndex = 0u;
    /** Number of constants to copy. */
    uint16_t count = 0u;
    /** Whether this constant range is
     *  dynamically indexed (float only) */
    bool isDynamicallyIndexed = false;
    /** Whether this constant range is sourced from
     *  shader defined data rather than API state.
     *  Only useful with dynamic indexing. */
    bool isShaderDefined = false;
  };


  /**
   * \brief Constant layout builder
   */
  class D3D9ConstantBufferLayout {

  public:

    D3D9ConstantBufferLayout();

    /**
     * \brief Builds trivial constant layout containing a set number of constants
     *
     * Assumes static indexing, and will not try to pack any gaps.
     * \param [in] maxCount Maximum available constant count
     */
    D3D9ConstantBufferLayout(uint32_t maxCount);

    /**
     * \brief Builds constant layout for static indexing
     *
     * \param [in] useLength Number of dwords in the constant mask
     * \param [in] useMask Bit mask of constants accessed by the shader
     */
    D3D9ConstantBufferLayout(uint32_t useLength, const uint32_t* useMask);

    /**
     * \brief Builds constant layout for dynamic indexing
     *
     * \param [in] maxCount Maximum available constant count
     * \param [in] defLength Number of dwords in shader-defined constant mask
     * \param [in] defMask Bit mask of shader-defined constants
     */
    D3D9ConstantBufferLayout(uint32_t maxCount, uint32_t defLength, const uint32_t* defMask);

    ~D3D9ConstantBufferLayout();

    /**
     * \brief Queries number of constant ranges
     * \returns Constant range count
     */
    uint32_t getRangeCount() const {
      return uint32_t(m_ranges.size());
    }

    /**
     * \brief Queries specific constant range
     *
     * \param [in] index Range index
     * \returns Given constant range
     */
    D3D9ConstantRange getRange(uint32_t index) const {
      return m_ranges[index];
    }

    /**
     * \brief Computes required constant count in buffer
     *
     * Multiply by constant size to get required buffer allocation size in bytes.
     * \param [in] valueCount Number of API constants with a non-default value.
     *    Useful to reduce the amount of memory used and copied for dynamic indexing
     *    if we can rely on robustness, but may be ignored or overridden if we do
     *    need to copy more data.
     * \returns Number of constants required
     */
    uint32_t computeConstantCount(uint32_t valueCount) const {
      return std::clamp(valueCount, m_minCount, m_maxCount);
    }

    /**
     * \brief Checks whether the layout is dynamically indexed
     * \returns \c true for dynamically indexed layouts
     */
    bool isDynamicallyIndexed() const {
      return m_dynamicIndexing;
    }

    /**
     * \brief Looks up remapped constant index by source index
     *
     * Only useful for statically indexed constant ranges.
     * \param [in] srcIndex Source constant index
     * \returns Constant index in buffer, if found
     */
    std::optional<uint32_t> findConstant(uint32_t srcIndex) const {
      if (!m_dynamicIndexing) {
        for (const auto& e : m_ranges) {
          if (srcIndex >= e.srcIndex && srcIndex < e.srcIndex + e.count)
            return std::make_optional(e.dstIndex + (srcIndex - e.srcIndex));
        }
      }

      return std::nullopt;
    }

  private:

    small_vector<D3D9ConstantRange, 16u> m_ranges;

    uint32_t m_minCount = 0u;
    uint32_t m_maxCount = 0u;

    bool m_dynamicIndexing = false;

    void addRange(const D3D9ConstantRange& range);

  };


  /**
   * \brief Required constant buffer sizes
   *
   * All sizes are given in bytes.
   */
  struct D3D9ConstantBufferSizes {
    uint32_t floatBufferSize  = 0u;
    uint32_t intBufferSize    = 0u;
    uint32_t boolBufferSize   = 0u;
  };


  /**
   * \brief Constant buffer copy arguments
   *
   * Points to buffer storage and source constants.
   */
  struct D3D9ConstantBufferCopyArgs {
    /** Pointer and size of float constant buffer */
    void* floatBuffer = nullptr;
    size_t floatBufferSize = 0u;
    /** Number of non-default API float constants */
    uint32_t floatConstantCount = 0u;
    /** Whether to replace NaN floats with 0.0 */
    bool flushNan = false;
    /** Pointer and size of integer constant buffer */
    void* intBuffer = nullptr;
    size_t intBufferSize = 0u;
    /** Pointer and size of bool constant buffer. For HWVP
     *  shaders, this shoud be ignored and set to null. */
    void* boolBuffer = nullptr;
    size_t boolBufferSize = 0u;
    /** Pointer to API-defined float constants */
    const Vector4* constFloatApi = nullptr;
    /** Pointer to shader-defined float constants */
    const Vector4* constFloatDef = nullptr;
    /** Pointer to API-defined integer constants */
    const Vector4i* constIntApi = nullptr;
    /** Pointer to API-defined boolean constants */
    const uint32_t* constBoolApi = nullptr;
  };


  /**
   * \brief Per-shader constant copy helper
   *
   * Allows efficiently copying shader constants to a constant buffer.
   */
  class D3D9ConstantBufferCopy {

  public:

    D3D9ConstantBufferCopy();

    D3D9ConstantBufferCopy(
            D3D9ConstantBufferLayout Floats,
            D3D9ConstantBufferLayout Ints,
            D3D9ConstantBufferLayout Bools);

    ~D3D9ConstantBufferCopy();

    /**
     * \brief Retrieces constant layouts of a given type
     */
    const D3D9ConstantBufferLayout& getLayout(D3D9ConstantType type) const {
      if (type == D3D9ConstantType::Float)
        return m_floatLayout;
      if (type == D3D9ConstantType::Int)
        return m_intLayout;
      return m_boolLayout;
    }

    /**
     * \brief Computes amount of data to allocate and write
     *
     * \param [in] FloatCount Float count for dynamic indexing
     * \returns Required constant buffer sizes for each type, in bytes
     */
    D3D9ConstantBufferSizes getAllocationSizes(uint32_t floatCount) const{
      D3D9ConstantBufferSizes result = {};
      result.floatBufferSize = m_floatLayout.computeConstantCount(floatCount) * sizeof(Vector4);
      result.intBufferSize = m_intLayout.computeConstantCount(0u) * sizeof(Vector4i);
      result.boolBufferSize = m_boolLayout.computeConstantCount(0u) * sizeof(uint32_t);
      return result;
    }

    /**
     * \brief Copies constant data to allocated buffers
     * \param [in] Args Constant pointers and copy parameters
     */
    void copyConstantData(const D3D9ConstantBufferCopyArgs& args) const;

  private:

    D3D9ConstantBufferLayout m_floatLayout  = {};
    D3D9ConstantBufferLayout m_intLayout    = {};
    D3D9ConstantBufferLayout m_boolLayout   = {};

    template<bool FlushNan>
    void writeFloatBuffer(const D3D9ConstantBufferCopyArgs& args) const;

    template<bool FlushNan>
    void writeFloatRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range, uint32_t maxCount) const;

    void writeIntBuffer(const D3D9ConstantBufferCopyArgs& args) const;

    void writeIntRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range) const;

    void writeBoolBuffer(const D3D9ConstantBufferCopyArgs& args) const;

    void writeBoolRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range) const;

    static void pad16(void* dst, size_t start, size_t end);

  };

}
