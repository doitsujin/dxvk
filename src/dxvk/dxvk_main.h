#pragma once

#include "../util/log/log.h"


namespace dxvk {
  
  /**
   * \brief Adds a message to the global DXVK log
   * \param [in] message Log message
   */
  void log(const std::string& message);
  
}