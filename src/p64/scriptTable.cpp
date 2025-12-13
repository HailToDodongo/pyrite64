// NOTE: Auto-Generated File!

#include "script/scriptTable.h"

namespace P64 { class Object; }

namespace P64::Script
{


  ScriptEntry codeTable[] = {

  };

  uint16_t codeSizeTable[] = {

  };

  constinit ScriptEntry codeDummy{};

  ScriptEntry& getCodeByIndex(uint32_t idx)
  {
    if (idx < sizeof(codeTable)/sizeof(codeTable[0])) {
      return codeTable[idx];
    }
    return codeDummy;
  }

  uint16_t getCodeSizeByIndex(uint32_t idx)
  {
    if (idx < sizeof(codeTable)/sizeof(codeTable[0])) {
      return codeSizeTable[idx];
    }
    return 0;
  }
}
