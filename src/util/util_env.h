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
  
  /**
   * \brief Gets the executable name
   * 
   * Returns the base name (not the full path) of the
   * program executable, including the file extension.
   * This function should be used to identify programs.
   * \returns Executable name
   */
  std::string getExeName();
  
  /**
   * \brief Gets the path to the dxvk specific temporary directory
   * 
   * Returns the path to the temporary directory for dxvk.
   * If no such directory can be found the string will be empty.
   * \returns Temporary directory
   */
  std::string getTempDirectory();
  
}
