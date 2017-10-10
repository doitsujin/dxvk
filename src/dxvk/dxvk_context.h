#pragma once

#include "dxvk_cmdlist.h"
#include "dxvk_framebuffer.h"

namespace dxvk {
  
  /**
   * \brief DXVk context
   * 
   * Tracks pipeline state and records command lists.
   * This is where the actual rendering commands are
   * recorded.
   */
  class DxvkContext : public RcObject {
    
  public:
    
    DxvkContext(const Rc<vk::DeviceFn>& vkd);
    ~DxvkContext();
    
    /**
     * \brief Begins command buffer recording
     * 
     * Begins recording a command list. This does
     * not alter any context state other than the
     * active command list.
     * \param [in] cmdList Target command list
     */
    void beginRecording(
      const Rc<DxvkCommandList>& cmdList);
    
    /**
     * \brief Ends command buffer recording
     * 
     * Finishes recording the active command list.
     * The command list can then be submitted to
     * the device.
     * 
     * The return value of this method can be used to
     * determine whether the command list needs to be
     * submitted. In case the command list is empty,
     * \c false will be returned and it shall not be
     * submitted to the device.
     * 
     * This will not change any context state
     * other than the active command list.
     * \returns \c true if any commands were recorded
     */
    bool endRecording();
    
    /**
     * \brief Sets framebuffer
     * \param [in] fb Framebuffer
     */
    void setFramebuffer(
      const Rc<DxvkFramebuffer>& fb);
    
  private:
    
    Rc<vk::DeviceFn>    m_vkd;
    Rc<DxvkCommandList> m_commandList;
    
  };
  
}