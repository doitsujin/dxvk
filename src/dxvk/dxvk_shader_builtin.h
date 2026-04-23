#pragma once

#include <ir/ir_builder.h>
#include <ir/ir_utils.h>

#include <spirv/spirv_builder.h>

#include "dxvk_device.h"
#include "dxvk_util.h"

using namespace dxbc_spv;

namespace dxvk {

  /**
   * \brief Built-in vertex shader info
   */
  struct DxvkBuiltInVertexShader {
    /* Vertex index, declared as a scalar uint. */
    ir::SsaDef vertexIndex = { };
    /* Instance index, declared as a scalar uint. */
    ir::SsaDef instanceIndex = { };
    /* Unscaled vertex coordinate, where a value of 0 corresponds to the
     * top-left corner of the viewport and 1 to the bottom-right. Defined
     * as a 2D float vector. */
    ir::SsaDef coord = { };
  };

  /**
   * \brief Built-in pixel shader info
   */
  struct DxvkBuiltInPixelShader {
    /* Sample ID input, relevant for MSAA use cases only. */
    ir::SsaDef sampleId = { };
  };

  /**
   * \brief Built-in compute shader info
   */
  struct DxvkBuiltInComputeShader {
    /* Global thread ID, as a 3D uint vector. */
    ir::SsaDef globalId = { };
    /* Workgroup ID, as a 3D uint vector. */
    ir::SsaDef groupId = { };
    /* Local thread ID, as a 3D uint vector. */
    ir::SsaDef localId = { };
    /* Flattened local thread index. */
    ir::SsaDef localIndex = { };
  };

  /**
   * \brief Trivial resource mapping for built-in shaders
   *
   * Bindings and push data layouts are explicit anyway, so the only
   * thing this actually needs to do is sort out the set index.
   */
  class DxvkBuiltInResourceMapping : public spirv::ResourceMapping {

  public:

    explicit DxvkBuiltInResourceMapping(const DxvkPipelineLayout* layout);

    ~DxvkBuiltInResourceMapping();

    spirv::DescriptorBinding mapDescriptor(
          ir::ScalarType          type,
          uint32_t                regSpace,
          uint32_t                regIndex) override;

    uint32_t mapPushData(ir::ShaderStageMask stages) override;

  private:

    uint32_t m_setIndex = 0u;

  };


  /**
   * \brief Built-in shader builder
   *
   * Provides common functionality to dynamically build dxbc-spirv IR
   * shaders for built-in operations.
   *
   * Shaders must be built in such a way that no legalization passes need
   * to be run, i.e. must be in SSA form and not use scoped control flow.
   * It is however allowed to use vectorized arithmetic since the builder
   * will run the scalarization pass. This is done for convenience.
   */
  class DxvkBuiltInShader {

  public:

    DxvkBuiltInShader(
            DxvkDevice*           device,
      const DxvkPipelineLayout*   layout,
      const std::string&          name);

    ~DxvkBuiltInShader();

    /**
     * \brief Creates empty compute shader template
     *
     * \param [in,out] builder Shader builder
     * \param [in] groupSize Workgroup size
     * \returns Compute shader metadata
     */
    DxvkBuiltInComputeShader buildComputeShader(
            ir::Builder&          builder,
            VkExtent3D            groupSize);

    /**
     * \brief Creates empty pixel shader template
     *
     * \param [in,out] builder Shader builder
     * \returns Pixel shader metadata
     */
    DxvkBuiltInPixelShader buildPixelShader(
            ir::Builder&          builder);

    /**
     * \brief Creates vertex shader for full-screen triangle
     *
     * \param [in,out] builder Shader builder
     * \returns Pixel shader metadata
     */
    DxvkBuiltInVertexShader buildFullscreenVertexShader(
            ir::Builder&          builder);

    /**
     * \brief Helper to declare an image SRV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] viewType Image view type
     * \param [in] viewFormat Expected format
     * \param [in] viewAspect Format aspect
     * \param [in] samples Expected sample count
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareImageSrv(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            VkImageViewType       viewType,
            VkFormat              viewFormat,
            VkImageAspectFlagBits viewAspect,
            VkSampleCountFlagBits samples);

    /**
     * \brief Helper to declare a typed buffer SRV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] viewFormat Expected format
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareTexelBufferSrv(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            VkFormat              viewFormat);

    /**
     * \brief Helper to declare a structured buffer SRV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] elementType Element type
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareBufferSrv(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            ir::BasicType         elementType);

    /**
     * \brief Helper to declare an image UAV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] viewType Image view type
     * \param [in] viewFormat Expected format
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareImageUav(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            VkImageViewType       viewType,
            VkFormat              viewFormat);

    /**
     * \brief Helper to declare a typed buffer UAV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] viewFormat Expected format
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareTexelBufferUav(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            VkFormat              viewFormat);

    /**
     * \brief Helper to declare a structured buffer UAV
     *
     * Also immediately inserts a descriptor load
     * at the current cursor location.
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] elementType Element type
     * \returns SSA def of the descriptor load.
     */
    ir::SsaDef declareBufferUav(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            ir::BasicType         elementType);

    /**
     * \brief Helper to declare a sampler index
     *
     * In helper shaders we declare samplers as \c uint32 since no
     * more than one sampler should be required anyway, if any.
     * Immediately inserts a descriptor load for the sampler.
     * \param [in,out] builder Shader builder
     * \param [in] pushDataOffset Push data offset, in bytes
     * \param [in] name Debug name
     * \returns SSA def of the descriptor load
     */
    ir::SsaDef declareSampler(
            ir::Builder&          builder,
            uint32_t              pushDataOffset,
      const char*                 name);

    /**
     * \brief Helper to declare an input target
     *
     * \param [in,out] builder Shader builder
     * \param [in] binding Binding index
     * \param [in] name Debug name
     * \param [in] attachment Render target index
     * \param [in] format Render target format
     * \param [in] samples Sample count
     * \returns SSA def of the descriptor load
     */
    ir::SsaDef declareInputTarget(
            ir::Builder&          builder,
            uint32_t              binding,
      const char*                 name,
            uint32_t              attachment,
            VkFormat              format,
            VkSampleCountFlagBits samples);

    /**
     * \brief Declares and loads push data parameter
     *
     * For graphics shaders, push data is unconditionally made
     * visible to both vertex and fragment shaders for simplicity.
     * \param [in,out] builder Shader builder
     * \param [in] type Push data type
     * \param [in] offset Push data offset, in bytes
     * \param [in] name Debug name
     * \returns SSA def of push data value
     */
    ir::SsaDef declarePushData(
            ir::Builder&          builder,
            ir::BasicType         type,
            uint32_t              pushDataOffset,
      const char*                 name);

    /**
     * \brief Loads built-in input variable
     *
     * Used to load vetex shader outputs in the pixel shader.
     * \param [in,out] builder Shader builder
     * \param [in] type Variable type
     * \param [in] builtIn Built-in type
     */
    ir::SsaDef declareBuiltInInput(
            ir::Builder&          builder,
            ir::BasicType         type,
            ir::BuiltIn           builtIn,
            ir::InterpolationModes interpolation = ir::InterpolationModes());

    /**
     * \brief Loads shader input variable
     *
     * Used to load vetex shader outputs in the pixel shader.
     * \param [in,out] builder Shader builder
     * \param [in] type Variable type
     * \param [in] location Output location
     * \param [in] name Debug name
     */
    ir::SsaDef declareInput(
            ir::Builder&          builder,
            ir::BasicType         type,
            uint32_t              location,
      const char*                 name,
            ir::InterpolationModes interpolation = ir::InterpolationModes());

    /**
     * \brief Exports built-in output
     *
     * Declares a built-in output variable and stores the given value.
     * \param [in,out] builder Shader builder
     * \param [in] builtIn Built-in to export
     * \param [in] value Layer index value
     */
    void exportBuiltIn(
            ir::Builder&          builder,
            ir::BuiltIn           builtIn,
            ir::SsaDef            value);

    /**
     * \brief Exports given output location
     *
     * Useful for built-in pixel shaders to write color output.
     * \param [in,out] builder Shader builder
     * \param [in] location Output location
     * \param [in] value Layer index value
     */
    void exportOutput(
            ir::Builder&          builder,
            uint32_t              location,
            ir::SsaDef            value,
      const char*                 name);

    /**
     * \brief Emits manual bound check
     *
     * \param [in,out] builder Shader builder
     * \param [in] coord Coordinate or index value
     * \param [in] size Size value
     * \param [in] dims Number of dimensions to consider
     * \returns SSA def of condition that will be \c true if all checked
     *    dimensions of \c coord are less than the corresponding \c size.
     */
    ir::SsaDef emitBoundCheck(
            ir::Builder&          builder,
            ir::SsaDef            coord,
            ir::SsaDef            size,
            uint32_t              dims);

    /**
     * \brief Emits conditional 'if' block
     *
     * Moves the cursor into the conditional block.
     * \param [in,out] builder Shader builder
     * \param [in] cond Condition that must be \c true to enter the block.
     * \returns SSA def of the merge block label.
     */
    ir::SsaDef emitConditionalBlock(
            ir::Builder&          builder,
            ir::SsaDef            cond);

    /**
     * \brief Extracts given number of dimensions from a vector
     *
     * \param [in,out] builder Shader builder
     * \param [in] vector Input vector
     * \param [in] first First dimension to extract
     * \param [in] count Number of dimensions
     * \returns Resulting scalar or vector
     */
    ir::SsaDef emitExtractVector(
            ir::Builder&          builder,
            ir::SsaDef            vector,
            uint32_t              first,
            uint32_t              count);

    /**
     * \brief Concatenates two values to a vector
     *
     * \param [in,out] builder Shader builder
     * \param [in] a First scalar or vector
     * \param [in] b Second scalar or vector
     * \returns Concatenated vector
     */
    ir::SsaDef emitConcatVector(
            ir::Builder&          builder,
            ir::SsaDef            a,
            ir::SsaDef            b);

    /**
     * \brief Replicates scalar into a vector
     *
     * \param [in,out] builder Shader builder
     * \param [in] type Vector type
     * \param [in] value Scalar value
     * \returns Concatenated vector
     */
    ir::SsaDef emitReplicateScalar(
            ir::Builder&          builder,
            ir::BasicType         type,
            ir::SsaDef            value);

    /**
     * \brief Masks output vector for a given format
     *
     * Replaces all color components that are not part of the given
     * format with zero. For depth or depth-stencil formats, this
     * will return a scalar value, otherwise a four-coponent vector.
     * \param [in,out] builder Shader builder
     * \param [in] format Format
     * \param [in] vector Vector
     * \returns Masked vector
     */
    ir::SsaDef emitFormatVector(
            ir::Builder&          builder,
            VkFormat              format,
            ir::SsaDef            a);

    /**
     * \brief Builds function for linear to sRGB conversion
     *
     * \param [in,out] builder Shader builder
     * \param [in] type Parameter and return type. If this is a
     *   four-component vector type, the last component will be
     *   passed through unmodified.
     * \returns Function that performs the conversion.
     */
    ir::SsaDef buildLinearToSrgbFn(
            ir::Builder&          builder,
      const ir::Type&             type);

    /**
     * \brief Prints disassembled shader to debug log
     *
     * Useful for debugging purposes.
     * \param [in] level Log level
     * \param [in] builder Shader builder
     */
    void printShader(
            LogLevel              level,
            ir::Builder&          builder);

    /**
     * \brief Builds SPIR-V shader
     *
     * Runs a minimal set of passes to eliminate unused inputs and converts
     * the shader to SPIR-V.
     * \param [in] builder Shader builder
     * \returns SPIR-V binary
     */
    std::vector<uint32_t> buildShader(
            ir::Builder&          builder);

    /**
     * \brief Determines scalar type for Vulkan image formats
     *
     * Returns either \c eF32, \c eU32 or \c eI32, depending on the format. For
     * depth aspects this will always return \c eF32, and \c eU32 for stencil.
     * \param [in] format Vulkan format to check
     * \param [in] aspect Image aspect to check
     * \returns Sampled type for the given format
     */
    ir::ScalarType determineSampledType(
            VkFormat              format,
            VkImageAspectFlagBits aspect);

    /**
     * \brief Determines resource kind for an image view type
     *
     * \param [in] viewType Image view type
     * \param [in] samples Image sample count
     * \returns Resource kind
     */
    ir::ResourceKind determineResourceKind(
            VkImageViewType       viewType,
            VkSampleCountFlagBits samples);

  private:

    DxvkShaderOptions m_options = { };

    DxvkBuiltInResourceMapping m_resourceMapping;

    std::string m_name;

    ir::SsaDef findEntryPoint(
            ir::Builder&          builder);

    ir::SsaDef findEntryPointFunction(
            ir::Builder&          builder);

    ir::SsaDef declareEntryPoint(
            ir::Builder&          builder,
            ir::ShaderStage       stage);

    ir::SsaDef splitLoad(
            ir::Builder&          builder,
            ir::OpCode            opCode,
            ir::SsaDef            def);

    ir::SsaDef makeConstantVector(
            ir::Builder&          builder,
            ir::SsaDef            constant,
            ir::BasicType         type);

    void splitStore(
            ir::Builder&          builder,
            ir::OpCode            opCode,
            ir::SsaDef            var,
            ir::SsaDef            value);

    void dumpShader(
            size_t                size,
      const uint32_t*             dwords);

  };

}
