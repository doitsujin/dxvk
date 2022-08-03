#pragma once

#include <type_traits>
#include <vector>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_texture.h"

namespace dxvk {

  class D3D11DeferredContext;
  class D3D11ImmediateContext;

  template<bool IsDeferred>
  struct D3D11ContextObjectForwarder;

  /**
   * \brief Object forwarder for immediate contexts
   *
   * Binding methods can use this to efficiently bind objects
   * to the DXVK context without redundant reference counting.
   */
  template<>
  struct D3D11ContextObjectForwarder<false> {
    template<typename T>
    static T&& move(T& object) {
      return std::move(object);
    }
  };

  /**
   * \brief Object forwarder for deferred contexts
   *
   * This forwarder will create a copy of the object passed
   * into it, so that CS chunks can be reused if necessary.
   */
  template<>
  struct D3D11ContextObjectForwarder<true> {
    template<typename T>
    static T move(const T& object) {
      return object;
    }
  };

  /**
   * \brief Common D3D11 device context implementation
   *
   * Implements all common device context methods, but since this is
   * templates with the actual context type (deferred or immediate),
   * all methods can call back into context-specific methods without
   * having to use virtual methods.
   */
  template<typename ContextType>
  class D3D11CommonContext : public D3D11DeviceContext {
    constexpr static bool IsDeferred = std::is_same_v<ContextType, D3D11DeferredContext>;
    using Forwarder = D3D11ContextObjectForwarder<IsDeferred>;
  public:
    
    D3D11CommonContext(
            D3D11Device*            pParent,
      const Rc<DxvkDevice>&         Device,
            DxvkCsChunkFlags        CsFlags);

    ~D3D11CommonContext();



  };
  
}
