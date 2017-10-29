#pragma once

#include <ostream>

#include "dxbc_enums.h"

std::ostream& operator << (std::ostream& os, dxvk::DxbcOpcode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcExtOpcode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandNumComponents e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandComponentSelectionMode e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandComponentName e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcOperandIndexRepresentation e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcResourceDim e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcResourceReturnType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcRegisterComponentType e);
std::ostream& operator << (std::ostream& os, dxvk::DxbcInstructionReturnType e);
