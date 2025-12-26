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

#define G_AC_NONE             (0 << G_MDSFT_ALPHACOMPARE)
#define G_AC_THRESHOLD        (1 << G_MDSFT_ALPHACOMPARE)
#define G_AC_DITHER           (3 << G_MDSFT_ALPHACOMPARE)

#define G_ZS_PIXEL            (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM             (1 << G_MDSFT_ZSRCSEL)

// H
#define G_MDSFT_BLENDMASK     0
#define G_MDSFT_ALPHADITHER   4
#define G_MDSFT_RGBDITHER     6

#define G_MDSFT_COMBKEY       8
#define G_MDSFT_TEXTCONV      9
#define G_MDSFT_TEXTFILT      12
#define G_MDSFT_TEXTLUT       14
#define G_MDSFT_TEXTLOD       16
#define G_MDSFT_TEXTDETAIL    17
#define G_MDSFT_TEXTPERSP     19
#define G_MDSFT_CYCLETYPE     20
#define G_MDSFT_COLORDITHER   22 // HW 1.0
#define G_MDSFT_PIPELINE      23

#define G_PM_1PRIMITIVE       (1 << G_MDSFT_PIPELINE)
#define G_PM_NPRIMITIVE       (0 << G_MDSFT_PIPELINE)

#define G_CYC_1CYCLE          (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE          (1 << G_MDSFT_CYCLETYPE)
#define G_CYC_COPY            (2 << G_MDSFT_CYCLETYPE)
#define G_CYC_FILL            (3 << G_MDSFT_CYCLETYPE)

#define G_TP_NONE             (0 << G_MDSFT_TEXTPERSP)
#define G_TP_PERSP            (1 << G_MDSFT_TEXTPERSP)

#define G_TD_CLAMP            (0 << G_MDSFT_TEXTDETAIL)
#define G_TD_SHARPEN          (1 << G_MDSFT_TEXTDETAIL)
#define G_TD_DETAIL           (2 << G_MDSFT_TEXTDETAIL)

#define G_TL_TILE             (0 << G_MDSFT_TEXTLOD)
#define G_TL_LOD              (1 << G_MDSFT_TEXTLOD)

#define G_TT_NONE             (0 << G_MDSFT_TEXTLUT)
#define G_TT_RGBA16           (1 << G_MDSFT_TEXTLUT)
#define G_TT_IA16             (2 << G_MDSFT_TEXTLUT)

#define G_TF_POINT            (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE          (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP           (2 << G_MDSFT_TEXTFILT)

#define G_TC_CONV             (0 << G_MDSFT_TEXTCONV)
#define G_TC_FILTCONV         (5 << G_MDSFT_TEXTCONV)
#define G_TC_FILT             (6 << G_MDSFT_TEXTCONV)

#define G_CK_NONE             (0 << G_MDSFT_COMBKEY)
#define G_CK_KEY              (1 << G_MDSFT_COMBKEY)

#define G_CD_MAGICSQ          (0 << G_MDSFT_RGBDITHER)
#define G_CD_BAYER            (1 << G_MDSFT_RGBDITHER)
#define G_CD_NOISE            (2 << G_MDSFT_RGBDITHER)
#define G_CD_DISABLE          (3 << G_MDSFT_RGBDITHER)
#define G_CD_ENABLE           G_CD_NOISE

#define G_AD_PATTERN          (0 << G_MDSFT_ALPHADITHER)
#define G_AD_NOTPATTERN       (1 << G_MDSFT_ALPHADITHER)
#define G_AD_NOISE            (2 << G_MDSFT_ALPHADITHER)
#define G_AD_DISABLE          (3 << G_MDSFT_ALPHADITHER)

// Draw flags (custom for the shader)
#define T3D_FLAG_DEPTH      (1 << 0)
#define T3D_FLAG_TEXTURED   (1 << 1)
#define T3D_FLAG_SHADED     (1 << 2)
#define T3D_FLAG_CULL_FRONT (1 << 3)
#define T3D_FLAG_CULL_BACK  (1 << 4)
#define T3D_FLAG_NO_LIGHT   (1 << 5)

#define DRAW_SHADER_COLLISION  (1 << 8)

/*
struct TileConf {
  vec2 mask;
  vec2 shift;
  vec2 low;
  vec2 high;
};
*/