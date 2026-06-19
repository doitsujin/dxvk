#include "d3d9_state.h"

#include "d3d9_texture.h"

namespace dxvk {

    template <template <typename T> typename ItemType>
    D3D9State<ItemType>::D3D9State() {
      for (uint32_t i = 0; i < streamFreq.size(); i++)
        streamFreq[i] = 1;
    }


    template <template <typename T> typename ItemType>
    D3D9State<ItemType>::~D3D9State() {
      if (textures) {
        for (uint32_t i = 0; i < textures->size(); i++)
          TextureChangePrivate(textures[i], nullptr);
      }
    }

    template struct D3D9State<dynamic_item>;
    template struct D3D9State<static_item>;

}
