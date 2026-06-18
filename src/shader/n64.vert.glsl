#version 460

layout (location = 0) in ivec4 inPosition;
//layout (location = 1) in vec2 inNorm;
layout (location = 1) in vec4 inColor;
layout (location = 2) in ivec2 inUV;
layout (location = 3) in ivec2 inBoneIdx;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec4 uv;
layout (location = 2) out flat uint v_objectID;
layout (location = 3) out flat vec4 tileSize;
layout (location = 4) out vec2 posScreen;
layout (location = 5) out noperspective vec4 cc_shade;
layout (location = 6) out flat vec4 cc_shade_flat;

#include "./defines.h"
#include "./ubo.glsl"

layout(std430, set = 0, binding = 0) readonly buffer BoneMatrices {
    mat4 boneMat[];
};

// set=3 in fragment shader
layout(std140, set = 1, binding = 0) uniform UniformGlobal {
    mat4 projMat;
    mat4 cameraMat;
    vec2 screenSize;
};

layout(std140, set = 1, binding = 1) uniform UniformObject {
  UBO_Material material;
};

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

mat3 calcNormalMax(in mat4 mat) {
  mat3 res = mat3(mat);
  res[0] = normalize(res[0]);
  res[1] = normalize(res[1]);
  res[2] = normalize(res[2]);
  return quantizeMat3(res);
}

void main()
{
  mat4 matM = material.modelMat;
  if(inBoneIdx.x >= 0) {
    matM = matM * boneMat[inBoneIdx.x];
  }

  mat4 matMV = quantizeMat4(cameraMat * matM);
  mat4 matMVP = projMat * matMV;

  mat3 matNormLight = calcNormalMax(matM);
  mat3 matNormScreen = calcNormalMax(matMV);

  vec2 uvPixel = (vec2(inUV) / float(1 << 5));
  v_objectID = material.objectID;

  vec3 inNormal = unpackNormals(inPosition.w);

  // Directional light
  vec3 norm = inNormal;
  vec3 normWorld = matNormLight * norm;
  vec3 normScreen = matNormScreen * norm;

  vec4 posWorld = matM * vec4(vec3(inPosition), 1.0);
  gl_Position = matMVP * vec4(vec3(inPosition), 1.0);
  posScreen = gl_Position.xy / gl_Position.w;

  cc_shade = inColor;

  //vec4 lightTotal = vec4(linearToGamma(material.ambientColor.rgb), 0.0);
  vec4 lightTotal = material.ambientColor;
  for(int i=0; i<6; ++i) {
    float pointLightSize = material.lightDir[i].w;
    float lightStren = 0;

    if(pointLightSize > 0.0) {
      vec3 ptPos = material.lightDir[i].xyz;
      vec3 toLight = ptPos - posWorld.xyz;
      float dist = length(toLight);
      toLight /= dist;

      lightStren = (pointLightSize / dist) * 0.5;
      lightStren *= lightStren;
      lightStren = clamp(lightStren, 0.0, 1.0);
      lightStren *= max(dot(normWorld, toLight), 0.0);
    } else {
      lightStren = max(dot(normWorld, material.lightDir[i].xyz), 0.0);
    }

    vec4 colorNorm = material.lightColor[i];
    lightTotal += colorNorm * lightStren;
  }

  lightTotal = clamp(lightTotal, 0.0, 1.0);
//  lightTotal.a = 1.0;

  vec4 addLight = clamp(cc_shade + lightTotal, 0.0, 1.0);
  vec4 mulLight = cc_shade * lightTotal;

  vec4 shadeWithLight = flagSelect(LIGHT_MODE_ADD, mulLight, addLight);

  cc_shade = flagSelect(T3D_FLAG_NO_LIGHT, shadeWithLight, cc_shade);
  //cc_shade.rgb = lightTotal.rgb;

  cc_shade = clamp(cc_shade, 0.0, 1.0);
  // cc_shade.rgb = norm * 0.5 + 0.5; // TEST
  cc_shade_flat = cc_shade;

  vec2 texSize = material.high.xy - material.low.xy + 1;
  texSize *= 0.5;
  normScreen.y = -normScreen.y;

  vec2 uvGen = normScreen.xy * texSize + (texSize*0.75);
  uvGen = vertexFxSelect(T3D_VERTEX_FX_SPHERICAL_UV, uvPixel, uvGen);

  // we simulate UVs in pixel-space, since there is only one UV set, we scale by the first texture size
  uv = uvGen.xyxy;// * texSize.xyxy;
  // apply material.shift from top left of texture:

  uv *= material.shift;

  uv = uv - (material.shift * 0.5) - material.low;

  tileSize = abs(material.high) - abs(material.low);

  if((DRAW_FLAGS & DRAW_SHADER_COLLISION) != 0) {
    cc_shade_flat.rgb = norm * 0.5 + 0.5;
    cc_shade_flat.a = 0.75;
    gl_Position.z -= 0.0001;
  }
}
