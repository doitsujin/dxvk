#pragma once

#include "util_string.h"

namespace dxvk::env {
  
  /**
   * \brief Gets environment variable
   * 
   * If the variable is not defined, this will return
   * an empty string. Note that environment variables
   * may be defined with an empty value.
   * \param [in] name Name of the variable
   * \returns Value of the variable
   */
  std::string getEnvVar(const wchar_t* name);
  
}