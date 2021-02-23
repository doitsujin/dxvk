#pragma once

#include "./hud/dxvk_hud.h"

#include "../dxvk/dxvk_context.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"

#include "../vulkan/vulkan_presenter.h"

namespace dxvk {

  /**
   * \brief Gamma control point
   */
  struct DxvkGammaCp {
    uint16_t r, g, b, a;
  };
  
  /**
   * \brief Presenter
   *
   * Common rendering code for presentation.
   */
  class DxvkPresenter : public RcObject {
    
  public:

    DxvkPresenter(
      const Rc<DxvkDevice>&     device,
            HWND                window,
      const vk::PresenterDesc&  desc,
      const std::string&        apiName);

    ~DxvkPresenter();

    /**
     * \brief Records command buffer
     *
     * \param [in] backBuffer Image to present
     * \param [in] dstRect Destination rectangle
     * \param [in] srcRect Back buffer rectangle
     * \param [in] signal Signal to signal
     * \param [in] frameId Frame number to signal
     * \param [in] repeat Number of times this frame has
     *   already been presented, for sync intervals > 1.
     * \returns \c true if a command list has been recorded
     */
    bool recordPresentCommands(
      const Rc<DxvkImageView>&  backBuffer,
            VkRect2D            dstRect,
            VkRect2D            srcRect,
      const Rc<sync::Signal>&   signal,
            uint32_t            frameId,
            uint32_t            repeat);

    /**
     * \brief Submits command buffer
     *
     * May be called from the CS thread. Call this immediately
     * after a successful \ref recordPresentCommands.
     * \param [in] repeat Number of times this frame has
     *   already been presented, for sync intervals > 1.
     */
    void submitPresentCommands(
            uint32_t            repeat);

    /**
     * \brief Changes swap chain parameters
     * \param [in] desc New parameters
     */
    void changeParameters(
      const vk::PresenterDesc&  desc);

    /**
     * \brief Sets gamma ramp
     *
     * If the number of control points is non-zero, this
     * will create a texture containing a gamma ramp that
     * will be used for presentation.
     * \param [in] cpCount Number of control points
     * \param [in] cpData Control point data
     */
    void setGammaRamp(
            uint32_t            cpCount,
      const DxvkGammaCp*        cpData);

    /**
     * \brief Adds a HUD item if enabled
     *
     * \param [in] name HUD item name
     * \param [in] args Constructor arguments
     */
    template<typename T, typename... Args>
    void addHudItem(const char* name, int32_t at, Args... args) {
      if (m_hud != nullptr)
        m_hud->addItem<T, Args...>(name, at, std::forward<Args>(args)...);
    }

    /**
     * \brief Checks whether a swap chain is present
     * \returns \c true if the presenter owns a Vulkan swap chain
     */
    bool hasSwapChain() {
      return m_presenter->hasSwapChain();
    }

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    struct PresenterArgs {
      VkOffset2D srcOffset;
      union {
        VkExtent2D srcExtent;
        VkOffset2D dstOffset;
      };
    };

    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    Rc<DxvkCommandList> m_commands;

    Rc<hud::Hud>        m_hud;

    Rc<DxvkShader>      m_fsCopy;
    Rc<DxvkShader>      m_fsBlit;
    Rc<DxvkShader>      m_fsResolve;
    Rc<DxvkShader>      m_vs;

    Rc<DxvkImage>       m_gammaImage;
    Rc<DxvkImageView>   m_gammaView;
    bool                m_gammaDirty = false;
    std::vector<DxvkGammaCp> m_gammaRamp;

    Rc<DxvkImage>       m_resolveImage;
    Rc<DxvkImageView>   m_resolveView;

    Rc<DxvkSampler>     m_samplerPresent;
    Rc<DxvkSampler>     m_samplerGamma;

    vk::PresenterDesc   m_presenterDesc;
    vk::PresenterSync   m_presenterSync;
    Rc<vk::Presenter>   m_presenter;
    DxvkSubmitStatus    m_status;
    
    std::vector<Rc<DxvkImageView>> m_renderTargetViews;

    void synchronizePresent();

    void createPresenter(HWND window);

    void createRenderTargetViews();

    void createSampler();

    void createShaders();

    void createResolveImage(
      const DxvkImageCreateInfo&  info);

    void destroyResolveImage();

    void recreateSwapChain();

  };
  
}