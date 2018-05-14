#pragma once

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>

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
