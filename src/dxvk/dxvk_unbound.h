#pragma once

#include "dxvk_buffer.h"
#include "dxvk_image.h"
#include "dxvk_sampler.h"

namespace dxvk {

  class DxvkContext;
  
  /**
   * \brief Unbound resources
   * 
   * Creates dummy resources that will be used
   * for descriptor sets when the client API did
   * not bind a compatible resource to a slot.
   */
  class DxvkUnboundResources {
    
  public:
    
    DxvkUnboundResources(DxvkDevice* dev);
    ~DxvkUnboundResources();
    
    /**
     * \brief Dummy buffer handle
     * 
     * Returns a handle to a buffer filled
     * with zeroes. Use for unbound vertex
     * and index buffers.
     * \returns Dummy buffer handle
     */
    VkBuffer bufferHandle() const {
      return m_buffer->getSliceHandle().handle;
    }
    
    /**
     * \brief Dummy buffer descriptor
     * 
     * Points to a small buffer filled with zeroes.
     * Do not write to this buffer, and do not use
     * it if out-of-bounds read access is possible.
     * \returns Dummy buffer descriptor
     */
    VkDescriptorBufferInfo bufferDescriptor() const {
      auto slice = m_buffer->getSliceHandle();
      
      VkDescriptorBufferInfo result;
      result.buffer = slice.handle;
      result.offset = slice.offset;
      result.range  = slice.length;
      return result;
    }
    
    /**
     * \brief Dummy buffer view
     * 
     * Returns an \c R32_UINT view into the
     * dummy buffer, which will contain one
     * element with undefined value.
     * \returns Dummy buffer view
     */
    VkBufferView bufferViewDescriptor() const {
      return m_bufferView->handle();
    }
    
    /**
     * \brief Dummy sampler descriptor
     * 
     * Points to a sampler which was created with
     * reasonable default values. Client APIs may
     * still require different behaviour.
     * \returns Dummy sampler descriptor
     */
    VkDescriptorImageInfo samplerDescriptor() const {
      VkDescriptorImageInfo result;
      result.sampler     = m_sampler->handle();
      result.imageView   = VK_NULL_HANDLE;
      result.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      return result;
    }
    
    /**
     * \brief Dummy combined image sampler descriptor
     * 
     * Contains both an image view and a sampler
     * descriptor for the given image view type.
     * \param [in] type Image view type
     * \returns Dummy image view descriptor
     */
    VkDescriptorImageInfo imageSamplerDescriptor(VkImageViewType type) const {
      auto view = getImageView(type, true);
      
      VkDescriptorImageInfo result;
      result.sampler     = m_sampler->handle();
      result.imageView   = view->handle();
      result.imageLayout = view->imageInfo().layout;
      return result;
    }
    
    /**
     * \brief Dummy image view descriptor
     * 
     * Points to an image view which, instead of
     * reading image data, will return zeroes for
     * all components unconditionally.
     * \param [in] type Image view type
     * \param [in] sampled Format selector
     * \returns Dummy image view descriptor
     */
    VkDescriptorImageInfo imageViewDescriptor(VkImageViewType type, bool sampled) const {
      auto view = getImageView(type, sampled);
      
      VkDescriptorImageInfo result;
      result.sampler     = VK_NULL_HANDLE;
      result.imageView   = view->handle();
      result.imageLayout = view->imageInfo().layout;
      return result;
    }
    
    /**
     * \brief Clears the resources
     * 
     * Initializes all images and buffers to zero.
     * \param [in] dev The DXVK device handle
     */
    void clearResources(DxvkDevice* dev);
    
  private:
    
    struct UnboundViews {
      Rc<DxvkImageView> view1D;
      Rc<DxvkImageView> view1DArr;
      Rc<DxvkImageView> view2D;
      Rc<DxvkImageView> view2DArr;
      Rc<DxvkImageView> viewCube;
      Rc<DxvkImageView> viewCubeArr;
      Rc<DxvkImageView> view3D;
    };
    
    Rc<DxvkSampler> m_sampler;
    
    Rc<DxvkBuffer>     m_buffer;
    Rc<DxvkBufferView> m_bufferView;
    
    Rc<DxvkImage> m_image1D;
    Rc<DxvkImage> m_image2D;
    Rc<DxvkImage> m_image3D;

    UnboundViews m_viewsSampled;
    UnboundViews m_viewsStorage;
    
    Rc<DxvkSampler> createSampler(DxvkDevice* dev);
    
    Rc<DxvkBuffer> createBuffer(DxvkDevice* dev);
    
    Rc<DxvkBufferView> createBufferView(
            DxvkDevice*     dev,
      const Rc<DxvkBuffer>& buffer);
    
    Rc<DxvkImage> createImage(
            DxvkDevice*     dev,
            VkImageType     type,
            uint32_t        layers);
    
    Rc<DxvkImageView> createImageView(
            DxvkDevice*     dev,
      const Rc<DxvkImage>&  image,
            VkFormat        format,
            VkImageViewType type,
            uint32_t        layers);

    UnboundViews createImageViews(
            DxvkDevice*     dev,
            VkFormat        format);

    const DxvkImageView* getImageView(
            VkImageViewType type,
            bool            sampled) const;
    
    void clearBuffer(
      const Rc<DxvkContext>&  ctx,
      const Rc<DxvkBuffer>&   buffer);
    
    void clearImage(
      const Rc<DxvkContext>&  ctx,
      const Rc<DxvkImage>&    image);
    
  };
  
}