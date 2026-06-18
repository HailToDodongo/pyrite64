#include "collision/gfxScale.h"

namespace P64::Coll {
    constexpr float DEFAULT_GFX_SCALE = 100.0f;

    static float gfxScale_{DEFAULT_GFX_SCALE};
    static float inverseGfxScale_{1.0f / DEFAULT_GFX_SCALE};

    float getGfxScale() { return gfxScale_; }
    float getInvGfxScale() { return inverseGfxScale_; }

    void setGfxScale(float gfxScale) {
        gfxScale_ = gfxScale;
        inverseGfxScale_ = 1.0f / gfxScale;
    }
}
