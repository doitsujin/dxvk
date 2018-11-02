#pragma once

/*
 * In 32-bit winelib build, alignment of Vulkan structures may be different than what
 * native C++ compiler expects. Wine exposes an extension, intended for winelib
 * applications, that exposes native Vulkan APIs with win32 additions, but using
 * native ABI.
 */
#ifdef __WINE__
#pragma push_macro("_WIN32")
#undef _WIN32
#endif

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>

#ifdef __WINE__
#pragma pop_macro("_WIN32")
#endif

#define VULKAN_FN(name) \
  VulkanFn<::PFN_ ## name> name = sym(#name)

namespace dxvk::vk {
  
  template<typename Fn>
  class VulkanFn;
  
  /**
   * \brief Vulkan function
   * 
   * Wraps an Vulkan function pointer and provides
   * a call operator using the correct types.
   */
  template<typename Ret, typename... Args>
  class VulkanFn<Ret (VKAPI_PTR*)(Args...)> {
    using Fn = Ret (VKAPI_PTR*)(Args...);
  public:
    
    VulkanFn() { }
    VulkanFn(Fn ptr)
    : m_fn(ptr) { }
    
    VulkanFn(PFN_vkVoidFunction ptr)
    : m_fn(reinterpret_cast<Fn>(ptr)) { }
    
    /**
     * \brief Invokes Vulkan function
     * 
     * \param [in] args Arguments
     * \returns Function return value
     */
    Ret operator () (Args... args) const {
      return (*m_fn)(args...);
    }
    
  private:
    
    Fn m_fn = nullptr;
    
  };
  
}
