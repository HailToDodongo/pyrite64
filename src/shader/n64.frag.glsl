#version 460

#include "./defines.h"
#include "./ubo.glsl"

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec4 uv;
layout (location = 2) in flat uint v_objectID;
layout (location = 3) in flat vec4 tileSize;
layout (location = 4) in vec2 posScreen;
layout (location = 5) in noperspective vec4 cc_shade;
layout (location = 6) in flat vec4 cc_shade_flat;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint ObjID;

layout(std140, set = 3, binding = 0) uniform UniformObject {
  UBO_Material material;
};

layout(set = 2, binding = 0) uniform sampler2D tex0;
layout(set = 2, binding = 1) uniform sampler2D tex1;

#include "./utils.glsl"

void fetchTex01Filtered(in ivec4 texSize, out vec4 texData0, out vec4 texData1)
{
  // Original 3-point code taken from: https://www.shadertoy.com/view/Ws2fWV (By: cyrb)
  ivec4 uv0 = ivec4(floor(uv));
  vec4 ratio = uv - vec4(uv0);

  ivec2 lower_flag = ivec2(step(1.0, ratio.xz + ratio.yw));
  ivec4 corner0 = ivec4(
    lower_flag.x, lower_flag.x,
    lower_flag.y, lower_flag.y
  );
  const ivec4 corner1 = ivec4(0,1,0,1);
  const ivec4 corner2 = ivec4(1,0,1,0);

  ivec4 uv1 = uv0 + corner0;
  ivec4 uv2 = uv0 + corner2;
  uv0 += corner1;

  uv0 = wrappedMirror(texSize, uv0);
  uv1 = wrappedMirror(texSize, uv1);
  uv2 = wrappedMirror(texSize, uv2);

  vec4 v0 = vec4(corner1 - corner0);
  vec4 v1 = vec4(corner2 - corner0);
  vec4 v2 = ratio - vec4(corner0);

  vec2 den = v0.xw * v1.yz - v1.xw * v0.yz;

  vec2 lambda1 = abs((v2.xz * v1.yw - v1.xz * v2.yw) / den);
  vec2 lambda2 = abs((v0.xz * v2.yw - v2.xz * v0.yw) / den);
  vec2 lambda0 = 1.0 - lambda1 - lambda2;

  texData0 =  texelFetch(tex0, uv1.xy, 0) * lambda0.x
            + texelFetch(tex0, uv0.xy, 0) * lambda1.x
            + texelFetch(tex0, uv2.xy, 0) * lambda2.x;

  texData1 =  texelFetch(tex1, uv1.zw, 0) * lambda0.y
            + texelFetch(tex1, uv0.zw, 0) * lambda1.y
            + texelFetch(tex1, uv2.zw, 0) * lambda2.y;
}

vec3 cc_fetchColor(in int val, in vec4 shade, in vec4 comb, in vec4 texData0, in vec4 texData1)
{
       if(val == CC_C_COMB       ) return comb.rgb;
  else if(val == CC_C_TEX0       ) return texData0.rgb;
  else if(val == CC_C_TEX1       ) return texData1.rgb;
  else if(val == CC_C_PRIM       ) return material.colPrim.rgb;
  else if(val == CC_C_SHADE      ) return shade.rgb;
  else if(val == CC_C_ENV        ) return material.colEnv.rgb;
  else if(val == CC_C_CENTER     ) return material.ck_center.rgb;
  else if(val == CC_C_SCALE      ) return material.ck_scale.rgb;
  else if(val == CC_C_COMB_ALPHA ) return comb.aaa;
  else if(val == CC_C_TEX0_ALPHA ) return texData0.aaa;
  else if(val == CC_C_TEX1_ALPHA ) return texData1.aaa;
  else if(val == CC_C_PRIM_ALPHA ) return material.colPrim.aaa;
  else if(val == CC_C_SHADE_ALPHA) return linearToGamma(shade.aaa);
  else if(val == CC_C_ENV_ALPHA  ) return material.colEnv.aaa;
  // else if(val == CC_C_LOD_FRAC   ) return vec3(0.0); // @TODO
  else if(val == CC_C_PRIM_LOD_FRAC) return vec3(material.primLodDepth[1]);
  else if(val == CC_C_NOISE      ) return vec3(noise(posScreen*0.25));
  else if(val == CC_C_K4         ) return vec3(material.k_45[0]);
  else if(val == CC_C_K5         ) return vec3(material.k_45[1]);
  else if(val == CC_C_1          ) return vec3(1.0);
  return vec3(0.0); // default: CC_C_0
}

float cc_fetchAlpha(in int val, vec4 shade, in vec4 comb, in vec4 texData0, in vec4 texData1)
{
       if(val == CC_A_COMB ) return comb.a;
  else if(val == CC_A_TEX0 ) return texData0.a;
  else if(val == CC_A_TEX1 ) return texData1.a;
  else if(val == CC_A_PRIM ) return material.colPrim.a;
  else if(val == CC_A_SHADE) return shade.a;
  else if(val == CC_A_ENV  ) return material.colEnv.a;
  // else if(val == CC_A_LOD_FRAC) return 0.0; // @TODO
  else if(val == CC_A_PRIM_LOD_FRAC) return material.primLodDepth[1];
  else if(val == CC_A_1    ) return 1.0;
  return 0.0; // default: CC_A_0
}

/**
 * handles both CC clamping and overflow:
 *        x ≤ -0.5: wrap around
 * -0.5 ≥ x ≤  1.5: clamp to 0-1
 *  1.5 < x       : wrap around
 */
vec4 cc_overflowValue(in vec4 value)
{
  return mod(value + 0.5, 2.0) - 0.5;
}
vec4 cc_clampValue(in vec4 value)
{
  return clamp(value, 0.0, 1.0);
}


vec4 blender_fetch(
  in int val, in vec4 colorBlend, in vec4 colorFog, in vec4 colorFB, in vec4 colorCC,
  in vec4 blenderA
)
{
       if (val == BLENDER_1      ) return vec4(1.0);
  else if (val == BLENDER_CLR_IN ) return colorCC;
  else if (val == BLENDER_CLR_MEM) return colorFB;
  else if (val == BLENDER_CLR_BL ) return colorBlend;
  else if (val == BLENDER_CLR_FOG) return colorCC;//colorFog; //@TODO: implemnent fog
  else if (val == BLENDER_A_IN   ) return colorCC.aaaa;
  else if (val == BLENDER_A_FOG  ) return colorFog.aaaa;
  else if (val == BLENDER_A_SHADE) return cc_shade.aaaa;
  else if (val == BLENDER_1MA    ) return 1.0 - blenderA.aaaa;
  else if (val == BLENDER_A_MEM  ) return colorFB.aaaa;
  return vec4(0.0); // default: BLENDER_0
}
/*
vec4 blendColor(in vec4 oldColor, vec4 newColor)
{
  vec4 colorBlend = vec4(0.0); // @TODO
  vec4 colorFog = vec4(1.0, 0.0, 0.0, 1.0); // @TODO

  vec4 P = blender_fetch(material.blender[0][0], colorBlend, colorFog, oldColor, newColor, vec4(0.0));
  vec4 A = blender_fetch(material.blender[0][1], colorBlend, colorFog, oldColor, newColor, vec4(0.0));
  vec4 M = blender_fetch(material.blender[0][2], colorBlend, colorFog, oldColor, newColor, A);
  vec4 B = blender_fetch(material.blender[0][3], colorBlend, colorFog, oldColor, newColor, A);

  vec4 res = ((P * A) + (M * B)) / (A + B);
  res.a = gammaToLinear(newColor.aaa).r; // preserve for 'A_IN'

  P = blender_fetch(material.blender[1][0], colorBlend, colorFog, oldColor, res, vec4(0.0));
  A = blender_fetch(material.blender[1][1], colorBlend, colorFog, oldColor, res, vec4(0.0));
  M = blender_fetch(material.blender[1][2], colorBlend, colorFog, oldColor, res, A);
  B = blender_fetch(material.blender[1][3], colorBlend, colorFog, oldColor, res, A);

  return ((P * A) + (M * B)) / (A + B);
}
*/
// This implements the part of the fragment shader that touches depth/color.
// All of this must happen in a way that guarantees coherency.
// This is either done via the shader interlock extension, or with a re-try loop as a fallback.
/*
bool color_depth_blending(
  in bool alphaTestFailed, in int writeDepth, in int currDepth, in vec4 ccValue,
  out uint oldColorInt, out uint writeColor
)
{
  ivec2 screenPosPixel = ivec2(trunc(gl_FragCoord.xy));
  int oldDepth = imageAtomicMax(depth_texture, screenPosPixel, writeDepth);
  int depthDiff = int(mixSelect(zSource() == G_ZS_PRIM, abs(oldDepth - currDepth), material.primLodDepth.w));

  bool depthTest = currDepth >= oldDepth;
  if((DRAW_FLAGS & DRAW_FLAG_DECAL) != 0) {
    depthTest = depthDiff <= DECAL_DEPTH_DELTA;
  }

  oldColorInt = imageLoad(color_texture, screenPosPixel).r;
  vec4 oldColor = unpackUnorm4x8(oldColorInt);
  oldColor.a = 0.0;

  vec4 ccValueBlended = blendColor(oldColor, vec4(ccValue.rgb, pow(ccValue.a, 1.0 / GAMMA_FACTOR)));

  bool shouldDiscard = alphaTestFailed || !depthTest;

  vec4 ccValueWrite = mixSelect(shouldDiscard, ccValueBlended, oldColor);
  ccValueWrite.a = 1.0;
  writeColor = packUnorm4x8(ccValueWrite);

  if(shouldDiscard)oldColorInt = writeColor;
  return shouldDiscard;
}
*/

void shaderCollision()
{
  if(!gl_FrontFacing) {
    discard;
  }

  FragColor = vec4(cc_shade_flat.rgb + 0.25, cc_shade_flat.a);
  ObjID = v_objectID;
}

void main()
{
  vec4 cc0[4]; // inputs for 1. cycle
  vec4 cc1[4]; // inputs for 2. cycle
  vec4 ccValue = vec4(0.0); // result after 1/2 cycleß

  vec4 uvTex = uv;

  if((DRAW_FLAGS & DRAW_SHADER_COLLISION) != 0) {
    shaderCollision();
    return;
  }

  if((DRAW_FLAGS & T3D_FLAG_CULL_BACK) != 0 && !gl_FrontFacing) {
    discard;
  }

  // @TODO: handle flat shading
  //vec4 ccShade = geoModeSelect(G_SHADE_SMOOTH, cc_shade_flat, cc_shade);
  vec4 ccShade = cc_shade;

  // pre-calc UVs for both textures + get two extra points for 3-point sampling
  // then sample both textures even if none are used. This is done to vectorize both
  // the UV and clamp/mirror calculations, as well as minimize the number of texture fetches
  vec4 texData0, texData1;
  ivec4 texSize = ivec4(textureSize(tex0, 0), textureSize(tex1, 0)) - 1;

  if(texFilter() == G_TF_BILERP)
  {
    fetchTex01Filtered(texSize, texData0, texData1);
  } else {
    ivec4 uv0 = ivec4(floor(uv + 0.5));
    uv0 = wrappedMirror(texSize, uv0);

    texData0 = texelFetch(tex0, uv0.xy, 0);
    texData1 = texelFetch(tex1, uv0.zw, 0);
  }

  // handle I4/I8
  //texData0.rgb = linearToGamma(texData0.rgb);
  //texData1.rgb = linearToGamma(texData1.rgb);
  //texData0.rgba = flagSelect(DRAW_FLAG_TEX0_MONO, texData0.rgba, texData0.rrrr);
  //texData1.rgba = flagSelect(DRAW_FLAG_TEX1_MONO, texData1.rgba, texData1.rrrr);

  // @TODO: emulate other formats, e.g. quantization?

  cc0[0].rgb = cc_fetchColor(material.cc0Color.x, ccShade, ccValue, texData0, texData1);
  cc0[1].rgb = cc_fetchColor(material.cc0Color.y, ccShade, ccValue, texData0, texData1);
  cc0[2].rgb = cc_fetchColor(material.cc0Color.z, ccShade, ccValue, texData0, texData1);
  cc0[3].rgb = cc_fetchColor(material.cc0Color.w, ccShade, ccValue, texData0, texData1);

  cc0[0].a = cc_fetchAlpha(material.cc0Alpha.x, ccShade, ccValue, texData0, texData1);
  cc0[1].a = cc_fetchAlpha(material.cc0Alpha.y, ccShade, ccValue, texData0, texData1);
  cc0[2].a = cc_fetchAlpha(material.cc0Alpha.z, ccShade, ccValue, texData0, texData1);
  cc0[3].a = cc_fetchAlpha(material.cc0Alpha.w, ccShade, ccValue, texData0, texData1);

  ccValue = cc_overflowValue((cc0[0] - cc0[1]) * cc0[2] + cc0[3]);

  if((OTHER_MODE_H & G_CYC_2CYCLE) != 0) {
    cc1[0].rgb = cc_fetchColor(material.cc1Color.x, ccShade, ccValue, texData0, texData1);
    cc1[1].rgb = cc_fetchColor(material.cc1Color.y, ccShade, ccValue, texData0, texData1);
    cc1[2].rgb = cc_fetchColor(material.cc1Color.z, ccShade, ccValue, texData0, texData1);
    cc1[3].rgb = cc_fetchColor(material.cc1Color.w, ccShade, ccValue, texData0, texData1);

    cc1[0].a = cc_fetchAlpha(material.cc1Alpha.x, ccShade, ccValue, texData0, texData1);
    cc1[1].a = cc_fetchAlpha(material.cc1Alpha.y, ccShade, ccValue, texData0, texData1);
    cc1[2].a = cc_fetchAlpha(material.cc1Alpha.z, ccShade, ccValue, texData0, texData1);
    cc1[3].a = cc_fetchAlpha(material.cc1Alpha.w, ccShade, ccValue, texData0, texData1);

    ccValue = (cc1[0] - cc1[1]) * cc1[2] + cc1[3];
  }

  ccValue = cc_clampValue(cc_overflowValue(ccValue));
  //ccValue.rgb = gammaToLinear(ccValue.rgb);

  // Depth / Decal handling:
  // We manually write & check depth values in an image in addition to the actual depth buffer.
  // This allows us to do manual compares (e.g. decals) and discard fragments based on that.
  // In order to guarantee proper ordering, we both use atomic image access as well as an invocation interlock per pixel.
  // If those where not used, a race-condition will occur where after a depth read happens, the depth value might have changed,
  // leading to culled faces writing their color values even though a new triangles has a closer depth value already written.
  // If no interlock is available, we use a re-try loop to ensure that the correct color value is written.
  // Note that this fallback can create small artifacts since depth and color are not able to be synchronized together.
  ivec2 screenPosPixel = ivec2(trunc(gl_FragCoord.xy));

  int currDepth = int(mixSelect(zSource() == G_ZS_PRIM, gl_FragCoord.w * 0xFFFFF, material.primLodDepth.z));
  //int writeDepth = int(flagSelect(DRAW_FLAG_DECAL, currDepth, -0xFFFFFF));
  int writeDepth = int(currDepth);

  /*if((DRAW_FLAGS & DRAW_FLAG_ALPHA_BLEND) != 0) {
    writeDepth = -0xFFFFFF;
  }*/

  bool alphaTestFailed = ccValue.a < ALPHA_CLIP;
  if(alphaTestFailed)writeDepth = -0xFFFFFF;

  #ifdef USE_SHADER_INTERLOCK
    beginInvocationInterlockARB();
  #else
    if(alphaTestFailed)discard; // discarding in interlock seems to cause issues, only do it here
  #endif

  FragColor = vec4(ccValue.rgb, ccValue.a);

  // hack for transparency, TODO: add proper blending support again
  if(material.blender.x == 0) {
    FragColor.a = 1.0f;
  }
  //FragColor.a = 1.0;
  // FragColor = cc_shade; // TEST

  /*{
    uint oldColorInt = 0;
    uint writeColor = 0;
    color_depth_blending(alphaTestFailed, writeDepth, currDepth, ccValue, oldColorInt, writeColor);

    #ifdef USE_SHADER_INTERLOCK
      imageAtomicCompSwap(color_texture, screenPosPixel, oldColorInt, writeColor.r);
    #else
      int count = 4;
      while(imageAtomicCompSwap(color_texture, screenPosPixel, oldColorInt, writeColor.r) != oldColorInt && count > 0)  {
        --count;
        if(color_depth_blending(alphaTestFailed, writeDepth, currDepth, ccValue, oldColorInt, writeColor)) {
          break;
        }
      }
      // Debug: write solod color in case we failed with our loop (seems to never happen)
      //if(count <= 0)imageAtomicExchange(color_texture, screenPosPixel, 0xFFFF00FF);
    #endif
  }*/

  // Since we only need our own depth/color textures, there is no need to actually write out fragments.
  // It may seem like we could use the calc. color from out texture and set it here,
  // but it will result in incoherent results (e.g. blocky artifacts due to depth related race-conditions)
  // This is most prominent on decals.
  // discard;

  ObjID = v_objectID;
}
