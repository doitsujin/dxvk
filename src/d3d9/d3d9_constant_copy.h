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

  struct D3D9ImmediateFloatConstant {
    uint32_t index;
    Vector4 value;
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

    bool eq(const D3D9ConstantRange& other) const {
      return dstIndex             == other.dstIndex
          && srcIndex             == other.srcIndex
          && count                == other.count
          && isDynamicallyIndexed == other.isDynamicallyIndexed
          && isShaderDefined      == other.isShaderDefined;
    }

    size_t hash() const {
      DxvkHashState hash;
      hash.add(dstIndex);
      hash.add(srcIndex);
      hash.add(count);
      hash.add(uint32_t(isDynamicallyIndexed));
      hash.add(uint32_t(isShaderDefined));
      return hash;
    }
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
     * \param [in] useLength Number of dwords in constant mask
     * \param [in] useMask Bit mask of statically indexed constants
     * \param [in] defData Actual shader-defined constant data
     */
    D3D9ConstantBufferLayout(
            uint32_t            maxCount,
            uint32_t            useLength,
      const uint32_t*           useMask,
            size_t              defCount,
      const D3D9ImmediateFloatConstant* defData);

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

    /**
     * \brief Retrieves shader-defined constants
     * \returns Shader-defined constants, if any
     */
    const Vector4* getShaderDefinedConstants() const {
      return m_constantData.data();
    }

    /**
     * \brief Checks whether two layouts fully match
     *
     * Two identical layouts can use the same constant buffers.
     * \param [in] other The other layout to check
     * \returns \c true if both layouts are identical
     */
    bool eq(const D3D9ConstantBufferLayout& other) const;

    /**
     * \brief Computes layout hash
     */
    size_t hash() const;

  private:

    small_vector<D3D9ConstantRange, 16u> m_ranges;
    small_vector<Vector4, 16u> m_constantData;

    uint32_t m_minCount = 0u;
    uint32_t m_maxCount = 0u;

    bool m_dynamicIndexing = false;

    void addRange(const D3D9ConstantRange& range);

    void addConstantData(
      const D3D9ConstantRange&          range,
            size_t                      defCount,
      const D3D9ImmediateFloatConstant* defData);

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
            D3D9ConstantBufferLayout floats,
            D3D9ConstantBufferLayout ints,
            D3D9ConstantBufferLayout bools);

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

    /**
     * \brief Checks whether two layouts fully match
     * \returns \c true if both layouts are identical
     */
    bool eq(const D3D9ConstantBufferCopy& other) const;

    size_t hash() const;

    /**
     * \brief Retrieves globally unique constant buffer layout
     *
     * The returned pointer will never be invalidated, and this function
     * will return the same pointer for compatible constant layouts.
     * \param [in] floats Float constant layout
     * \param [in] ints Integer constant layout
     * \param [in] bools Boolean constant layout
     * \returns Pointer to unique layout
     */
    static const D3D9ConstantBufferCopy* getOrCreate(
            D3D9ConstantBufferLayout floats,
            D3D9ConstantBufferLayout ints,
            D3D9ConstantBufferLayout bools);

  private:

    D3D9ConstantBufferLayout m_floatLayout  = {};
    D3D9ConstantBufferLayout m_intLayout    = {};
    D3D9ConstantBufferLayout m_boolLayout   = {};

    static dxvk::mutex s_mutex;
    static std::unordered_set<D3D9ConstantBufferCopy, DxvkHash, DxvkEq> s_layouts;

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
