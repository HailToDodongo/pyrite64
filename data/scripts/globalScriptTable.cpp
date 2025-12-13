// NOTE: Auto-Generated File!

#include <assert.h>

#include "script/globalScript.h"

namespace P64::GlobalScript
{
  __CODE_DECL__

  void callHooks(HookType type)
  {
    switch (type)
    {
      __CODE_HOOKS__

      default: break;
    }
  }
}
