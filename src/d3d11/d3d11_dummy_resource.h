#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  /**
   * \brief D3D11 dummy resources
   * 
   * Binding dummy resources to resource slots is
   * required in cases where the application binds
   * \c nullptr in order to keep the backend alive.
   */
  struct D3D11DummyResources : public RcObject {
    D3D11DummyResources(
      const Rc<DxvkDevice>&       device,
            VkPipelineStageFlags  enabledShaderStages);
    ~D3D11DummyResources();
    
    Rc<DxvkSampler>     sampler;            ///< Dummy texture sampler
    Rc<DxvkBuffer>      buffer;             ///< Dummy constant/vertex buffer
    Rc<DxvkBufferView>  bufferView;         ///< Dummy buffer SRV or UAV
    
    Rc<DxvkImage>       image1D;            ///< Dummy 1D image, used to back 1D and 1D Array views
    Rc<DxvkImage>       image2D;            ///< Dummy 2D image, used to back 2D, 2D Array and Cube views
    Rc<DxvkImage>       image3D;            ///< Dummy 3D image, used to back the 3D view
    
    Rc<DxvkImageView>   imageView1D;        ///< 1D view
    Rc<DxvkImageView>   imageView1DArray;   ///< 1D array view
    Rc<DxvkImageView>   imageView2D;        ///< 2D view
    Rc<DxvkImageView>   imageView2DArray;   ///< 2D array view
    Rc<DxvkImageView>   imageViewCube;      ///< 2D cube view
    Rc<DxvkImageView>   imageViewCubeArray; ///< 2D cube array view
    Rc<DxvkImageView>   imageView3D;        ///< 3D view
  };
  
}