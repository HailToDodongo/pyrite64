#version 460

layout (location = 0) in ivec3 inPosition;
//layout (location = 1) in vec2 inNorm;
layout (location = 1) in vec4 inColor;
layout (location = 2) in uvec2 inUV;

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec2 v_uv;
layout (location = 2) out flat uint v_objectID;

layout (location = 3) out flat ivec4 v_cc0Color;
layout (location = 4) out flat ivec4 v_cc0Alpha;
layout (location = 5) out flat ivec4 v_cc1Color;
layout (location = 6) out flat ivec4 v_cc1Alpha;

// set=3 in fragment shader
layout(std140, set = 1, binding = 0) uniform UniformGlobal {
    mat4 projMat;
    mat4 cameraMat;
};

layout(std140, set = 1, binding = 1) uniform UniformObject {
    mat4 modelMat;

    uint cc0Color;
    uint cc0Alpha;
    uint cc1Color;
    uint cc1Alpha;
    uint colPrim;
    uint colEnv;

    uint objectID;
};

void main()
{
  mat4 matMVP = projMat * cameraMat * modelMat;
  vec3 posNorm = vec3(inPosition) / 32768.0;
  //posNorm /= 32768.0;
  posNorm *= 64;

  gl_Position = matMVP * vec4(posNorm, 1.0);

  v_cc0Color = ivec4(unpackUnorm4x8(cc0Color) * 255.0);
  v_cc0Alpha = ivec4(unpackUnorm4x8(cc0Alpha) * 255.0);
  v_cc1Color = ivec4(unpackUnorm4x8(cc1Color) * 255.0);
  v_cc1Alpha = ivec4(unpackUnorm4x8(cc1Alpha) * 255.0);

  vec4 prinColor = unpackUnorm4x8(colPrim);

  v_color = inColor * prinColor;// * vec4(test, 1.0f);
  v_uv = vec2(inUV) / float(1 << 5);
  v_objectID = objectID;
}
