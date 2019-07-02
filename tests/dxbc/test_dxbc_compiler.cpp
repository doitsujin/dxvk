#include <iterator>
#include <fstream>

#include "../../src/dxbc/dxbc_module.h"
#include "../../src/dxvk/dxvk_shader.h"

#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

namespace dxvk {
  Logger Logger::s_instance("dxbc-compiler.log");
}

using namespace dxvk;

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  int     argc = 0;
  LPWSTR* argv = CommandLineToArgvW(
    GetCommandLineW(), &argc);  
  
  if (argc < 3) {
    Logger::err("Usage: dxbc-compiler input.dxbc output.spv");
    return 1;
  }
  
  try {
    std::string ifileName = str::fromws(argv[1]);
    std::ifstream ifile(ifileName, std::ios::binary);
    ifile.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize length = ifile.gcount();
    ifile.clear();
    
    ifile.seekg(0, std::ios_base::beg);
    std::vector<char> dxbcCode(length);
    ifile.read(dxbcCode.data(), length);
    
    DxbcReader reader(dxbcCode.data(), dxbcCode.size());
    DxbcModule module(reader);
    
    DxbcModuleInfo moduleInfo;
    moduleInfo.options.useSubgroupOpsForAtomicCounters = true;
    moduleInfo.options.useDemoteToHelperInvocation = true;
    moduleInfo.options.minSsboAlignment = 4;
    moduleInfo.xfb = nullptr;

    Rc<DxvkShader> shader = module.compile(moduleInfo, ifileName);
    std::ofstream ofile(str::fromws(argv[2]), std::ios::binary);
    shader->dump(ofile);
    return 0;
  } catch (const DxvkError& e) {
    Logger::err(e.message());
    return 1;
  }
}
