#pragma once
#include <libdragon.h>

#define assert_text "gfxScale has been removed in favor of using meters as the unit for all lengths, so this function is no longer needed. \
Please remove all calls to it and any imports & references to gfxScale in your code. See also: renderScale.h for the new render-scale system, that scales at the boundaries between the engine and the RSP"

namespace P64::Coll
{
    
    [[deprecated("gfxScale has been removed in favor of using meters as the unit for all lengths")]]
    constexpr float getGfxScale()
    {
        static_assert(false, assert_text);
        return 0;
    }

    [[deprecated("gfxScale has been removed in favor of using meters as the unit for all lengths")]]
    constexpr float getInvGfxScale()
    {
        static_assert(false, assert_text);
        return 0;
    }

    [[deprecated("gfxScale has been removed in favor of using meters as the unit for all lengths")]]
    constexpr void setGfxScale(float gfxScale)
    {
        static_assert(false, assert_text);
    }
}