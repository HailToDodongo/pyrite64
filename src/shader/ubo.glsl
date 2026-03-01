struct UBO_Material {
  mat4 modelMat;

  //Tile settings: xy = TEX0, zw = TEX1
  vec4 mask; // clamped if < 0, mask = abs(mask)
  vec4 shift;
  vec4 low;
  vec4 high; // if negative, mirrored, high = abs(high)

  // color-combiner
  ivec4 cc0Color;
  ivec4 cc0Alpha;
  ivec4 cc1Color;
  ivec4 cc1Alpha;

  ivec4 modes; // vertexFX, other-low, other-high, flags

  vec4 lightColor[2];
  vec4 lightDir[2]; // [0].w is alpha clip
  vec4 colPrim;
  vec4 colEnv;
  vec4 ambientColor;
  vec4 ck_center;
  vec4 ck_scale;
  vec4 primLodDepth;
  vec4 k_0123;
  vec2 k_45;

  ivec2 blender;

  uint objectID;
};