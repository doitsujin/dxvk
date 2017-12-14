#pragma once

#include <ostream>

#include "dxbc_common.h"
#include "dxbc_enums.h"

std::ostream& operator << (std::ostream& os, dxvk::DxbcOpcode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcExtOpcode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandExt e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcComponentCount e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcRegMode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandIndexRepresentation e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcResourceDim e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcResourceReturnType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcRegisterComponentType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcInstructionReturnType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcSystemValue e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcProgramType e);
