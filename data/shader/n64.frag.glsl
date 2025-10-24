#version 460

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in flat uint v_objectID;

layout (location = 3) in flat ivec4 v_cc0Color;
layout (location = 4) in flat ivec4 v_cc0Alpha;
layout (location = 5) in flat ivec4 v_cc1Color;
layout (location = 6) in flat ivec4 v_cc1Alpha;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint ObjID;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main()
{
  ivec2 uvNorm = ivec2(v_uv);
  // tex-size:

  ivec2 texSize = textureSize(texSampler, 0);
  // repeat
  uvNorm = ivec2(mod(uvNorm.x, texSize.x), mod(uvNorm.y, texSize.y));

  FragColor = texelFetch(texSampler, uvNorm, 0) * v_color;

  //FragColor = v_color;
  ObjID = v_objectID;
}
