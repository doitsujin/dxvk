#pragma once

#include "dxvk_resource.h"

namespace dxvk {

  /**
   * \brief Marker
   *
   * Arbitrary marker that can be used to track whether
   * the GPU has finished processing certain commands,
   * and stores some data.
   */
  template<typename T>
  class DxvkMarker : public DxvkResource {

  public:

    DxvkMarker(T&& payload)
    : m_payload(std::move(payload)) { }

    DxvkMarker(const T& payload)
    : m_payload(payload) { }

    const T& payload() const {
      return m_payload;
    }

  private:

    T m_payload;

  };

}
