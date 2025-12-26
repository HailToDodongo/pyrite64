#version 460

layout (location = 0) in ivec4 inPosition;
//layout (location = 1) in vec2 inNorm;
layout (location = 1) in vec4 inColor;
layout (location = 2) in ivec2 inUV;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec4 uv;
layout (location = 2) out flat uint v_objectID;
layout (location = 3) out flat vec4 tileSize;
layout (location = 4) out vec2 posScreen;
layout (location = 5) out noperspective vec4 cc_shade;
layout (location = 6) out flat vec4 cc_shade_flat;

#include "./defines.h"
#include "./ubo.glsl"

// set=3 in fragment shader
layout(std140, set = 1, binding = 0) uniform UniformGlobal {
    mat4 projMat;
    mat4 cameraMat;
    vec2 screenSize;
};

layout(std140, set = 1, binding = 1) uniform UniformObject {
  UBO_Material material;
};

layout(set = 2, binding = 0) uniform sampler2D tex0;
layout(set = 2, binding = 1) uniform sampler2D tex1;

#include "./utils.glsl"

vec3 unpackNormals(int packed)
{
  // extract bits
  ivec3 comp = packed.xxx >> ivec3(11, 5, 0);
  comp &= ivec3(0x1F, 0x3F, 0x1F);
  // sign extend
  comp = (comp << ivec3(27, 26, 27)) >> ivec3(27, 26, 27);
  // normalize to [-1, 1]
  return vec3(comp) / vec3(15.0, 31.0, 15.0);
}

void main()
{
  mat4 matMV = quantizeMat4(cameraMat * material.modelMat);
  mat4 matMVP = projMat * matMV;

  mat3 matNorm = mat3(material.modelMat);
  matNorm[0] = normalize(matNorm[0]);
  matNorm[1] = normalize(matNorm[1]);
  matNorm[2] = normalize(matNorm[2]);
  matNorm = quantizeMat3(matNorm);

  vec2 uvPixel = (vec2(inUV) / float(1 << 5));
  v_objectID = material.objectID;

  vec3 inNormal = unpackNormals(inPosition.w);

  // Directional light
  vec3 norm = inNormal;
  vec3 normScreen = matNorm * norm;

  cc_shade = inColor;

  //vec4 lightTotal = vec4(linearToGamma(material.ambientColor.rgb), 0.0);
  vec4 lightTotal = material.ambientColor;
  for(int i=0; i<2; ++i) {
    float lightStren = max(dot(normScreen, material.lightDir[i].xyz), 0.0);
    //vec4 colorNorm = vec4(linearToGamma(material.lightColor[i].rgb), 1.0);
    vec4 colorNorm = material.lightColor[i];
    lightTotal += colorNorm * lightStren;
  }

  lightTotal = clamp(lightTotal, 0.0, 1.0);
//  lightTotal.a = 1.0;

  vec4 shadeWithLight = cc_shade * lightTotal;

  cc_shade = flagSelect(T3D_FLAG_NO_LIGHT, shadeWithLight, cc_shade);
  //cc_shade.rgb = shadeWithLight;

  cc_shade = clamp(cc_shade, 0.0, 1.0);
  // cc_shade.rgb = norm * 0.5 + 0.5; // TEST
  cc_shade_flat = cc_shade;

  // @TODO: handle vertex effect
  vec2 uvGen = uvPixel;// geoModeSelect(G_TEX_GEN, uvPixel, normScreen.xy * 0.5 + 0.5);

  // turn UVs ionto pixel-space, apply first tile settings
  ivec4 texSize = ivec4(textureSize(tex0, 0), textureSize(tex1, 0));
  // we simulate UVs in pixel-space, since there is only one UV set, we scale by the first texture size
  uv = uvGen.xyxy;// * texSize.xyxy;
  // apply material.shift from top left of texture:

  uv *= material.shift;

  uv = uv - (material.shift * 0.5) - material.low;

  tileSize = abs(material.high) - abs(material.low);

  // @TODO: uvgen (f3d + t3d)
  // forward CC (@TODO: do part of this here? e.g. prim/env/shade etc.)

  gl_Position = matMVP * vec4(vec3(inPosition), 1.0);
  posScreen = gl_Position.xy / gl_Position.w;

  if((DRAW_FLAGS & DRAW_SHADER_COLLISION) != 0) {
    cc_shade_flat.rgb = norm * 0.5 + 0.5;
    cc_shade_flat.a = 0.75;
    gl_Position.z -= 0.0001;
  }
}
