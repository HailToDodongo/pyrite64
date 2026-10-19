// NOTE: Auto-Generated File!

#include <script/scriptTable.h>
#include <script/nodeGraph.h>

namespace P64 { class Object; }

namespace P64::NodeGraph {
__GRAPH_DEF__
}

namespace P64::Script
{
__CODE_DECL__

  ScriptEntry codeTable[] = {
__CODE_ENTRIES__
  };

  uint16_t codeSizeTable[] = {
__CODE_SIZE_ENTRIES__
  };

  constinit const void* codeDummyType = nullptr;
  constinit ScriptEntry codeDummy{.dataType = &codeDummyType};

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

  NodeGraph::GraphFunc getGraphFuncByUUID(uint64_t uuid)
  {
    switch (uuid)
    {
__GRAPH_SWITCH_CASE__
      default: return nullptr;
    }
  }
}
