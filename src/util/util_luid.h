#include "./com/com_include.h"

namespace dxvk {

  /**
   * \brief Retrieves an adapter LUID
   * 
   * Note that this only works reliably within the
   * module that this function was compiled into.
   * \param [in] Adapter The adapter index
   * \returns LUID An LUID for that adapter
   */
  LUID GetAdapterLUID(UINT Adapter);

}
