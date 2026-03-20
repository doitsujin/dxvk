#pragma once

#include <stdexcept>
#include <string>

namespace dxvk {
  
  /**
   * \brief DXVK error
   * 
   * A generic exception class that stores a
   * message. Exceptions should be logged.
   *
   * GeneralsX @bugfix fbraz3 20/03/2026 Inherit std::exception so DxvkError is
   * caught by catch(const std::exception&) handlers in the game, exposing the
   * actual Vulkan/DXVK error message instead of "unknown exception".
   */
  class DxvkError : public std::exception {
    
  public:
    
    DxvkError() { }
    DxvkError(std::string&& message)
    : m_message(std::move(message)) { }
    
    const std::string& message() const {
      return m_message;
    }

    const char* what() const noexcept override {
      return m_message.c_str();
    }
    
  private:
    
    std::string m_message;
    
  };
  
}