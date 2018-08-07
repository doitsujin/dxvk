#pragma once

#include "../util_env.h"

#include "config.h"

namespace dxvk {

  /**
   * \brief Retrieves application defaults
   * 
   * Some apps have options enabled by default
   * in order to improve compatibility and/or
   * performance.
   * \param [in] appName Application name
   * \returns Default configuration for the app
   */
  Config getAppConfig(const std::string& appName);

  /**
   * \brief Retrieves user configuration
   * 
   * Opens and parses the file \c dxvk.conf if it
   * exists, or whatever file name is specified in
   * the environment variable \c DXVK_CONFIG_FILE.
   * \returns User configuration
   */
  Config getUserConfig();

}