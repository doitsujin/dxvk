#include <iostream>
#include <string>

#include <d3dcompiler.h>

#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

#include "../../src/util/com/com_pointer.h"

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  int     argc = 0;
  LPWSTR* argv = CommandLineToArgvW(
    GetCommandLineW(), &argc);

  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: dxbc-disasm input.dxbc [output]" << std::endl;
    return 1;
  }

  Com<ID3DBlob> assembly;
  Com<ID3DBlob> binary;

  // input file
  if (FAILED(D3DReadFileToBlob(argv[1], &binary))) {
    std::cerr << "Failed to read shader" << std::endl;
    return 1;
  }

  HRESULT hr = D3DDisassemble(
    binary->GetBufferPointer(),
    binary->GetBufferSize(),
    D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING, nullptr,
    &assembly);

  if (FAILED(hr)) {
    std::cerr << "Failed to disassemble shader" << std::endl;
    return 1;
  }

  // output file variant
  if (argc == 3 && FAILED(D3DWriteBlobToFile(assembly.ptr(), argv[2], 1))) {
    std::cerr << "Failed to write shader" << std::endl;
    return 1;
  }

  // stdout variant
  if (argc == 2) {
    std::string data((const char *)assembly->GetBufferPointer(), assembly->GetBufferSize());
    std::cout << data;
  }

  return 0;
}
