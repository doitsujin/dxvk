#pragma once

#include <ostream>
#include <iomanip>

#include "com_include.h"

#if !defined(_MSC_VER) && !defined(DXVK_NATIVE)
# ifdef __WINE__
#   define DXVK_DEFINE_GUID(iface) \
      template<> inline GUID const& __wine_uuidof<iface> () { return iface::guid; }
# else
#   define DXVK_DEFINE_GUID(iface) \
      template<> inline GUID const& __mingw_uuidof<iface> () { return iface::guid; }
# endif
#endif

std::ostream& operator << (std::ostream& os, REFIID guid);
