// CC Color inputs
#define CC_C_0             0
#define CC_C_1             1
#define CC_C_COMB          2
#define CC_C_TEX0          3
#define CC_C_TEX1          4
#define CC_C_PRIM          5
#define CC_C_SHADE         6
#define CC_C_ENV           7
#define CC_C_CENTER        8
#define CC_C_SCALE         9
#define CC_C_COMB_ALPHA    10
#define CC_C_TEX0_ALPHA    11
#define CC_C_TEX1_ALPHA    12
#define CC_C_PRIM_ALPHA    13
#define CC_C_SHADE_ALPHA   14
#define CC_C_ENV_ALPHA     15
#define CC_C_LOD_FRAC      16
#define CC_C_PRIM_LOD_FRAC 17
#define CC_C_NOISE         18
#define CC_C_K4            19
#define CC_C_K5            20

// CC Alpha inputs
#define CC_A_0             0
#define CC_A_1             1
#define CC_A_COMB          2
#define CC_A_TEX0          3
#define CC_A_TEX1          4
#define CC_A_PRIM          5
#define CC_A_SHADE         6
#define CC_A_ENV           7
#define CC_A_LOD_FRAC      8
#define CC_A_PRIM_LOD_FRAC 9

// Blender inputs
#define BLENDER_0       0
#define BLENDER_1       1
#define BLENDER_CLR_IN  2
#define BLENDER_CLR_MEM 3
#define BLENDER_CLR_BL  4
#define BLENDER_CLR_FOG 5
#define BLENDER_A_IN    6
#define BLENDER_A_FOG   7
#define BLENDER_A_SHADE 8
#define BLENDER_1MA     9
#define BLENDER_A_MEM   10

// Geometry modes (some of these should be removed)
#define DRAWFLAG_DEPTH      (1 << 0)
#define DRAWFLAG_TEXTURED   (1 << 1)
#define DRAWFLAG_SHADED     (1 << 2)
#define DRAWFLAG_CULL_FRONT (1 << 3)
#define DRAWFLAG_CULL_BACK  (1 << 4)
#define DRAWFLAG_NO_LIGHT   (1 << 16)

// Othermode modes
// L
#define G_MDSFT_ALPHACOMPARE  0
#define G_MDSFT_ZSRCSEL       2
#define G_MDSFT_RENDERMODE    3
#define G_MDSFT_BLENDER       16

#define G_ZS_PRIM             (1 << G_MDSFT_ZSRCSEL)

#define G_MDSFT_TEXTFILT      12
#define G_MDSFT_TEXTPERSP     19
#define G_MDSFT_CYCLETYPE     20

#define G_CYC_1CYCLE          (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE          (1 << G_MDSFT_CYCLETYPE)

#define G_TF_POINT            (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE          (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP           (2 << G_MDSFT_TEXTFILT)


// Draw flags (custom for the shader)
#define T3D_FLAG_DEPTH      (1 << 0)
#define T3D_FLAG_TEXTURED   (1 << 1)
#define T3D_FLAG_SHADED     (1 << 2)
#define T3D_FLAG_CULL_FRONT (1 << 3)
#define T3D_FLAG_CULL_BACK  (1 << 4)
#define T3D_FLAG_NO_LIGHT   (1 << 16)

#define T3D_VERTEX_FX_NONE           0
#define T3D_VERTEX_FX_SPHERICAL_UV   1
#define T3D_VERTEX_FX_CELSHADE_COLOR 2
#define T3D_VERTEX_FX_CELSHADE_ALPHA 3
#define T3D_VERTEX_FX_OUTLINE        4
#define T3D_VERTEX_FX_UV_OFFSET      5

#define DRAW_SHADER_COLLISION  (1 << 8)

/*
struct TileConf {
  vec2 mask;
  vec2 shift;
  vec2 low;
  vec2 high;
};
*/