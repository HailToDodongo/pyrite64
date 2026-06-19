// Value-pin data types and the conversions allowed between them.

valueType("i32",  { name: "Int",    cType: "int32_t",   default: "0",    color: [0x7E, 0xC8, 0x4E], size: 4  });
valueType("u32",  { name: "UInt",   cType: "uint32_t",  default: "0",    color: [0x4D, 0xC0, 0xB5], size: 4  });
valueType("f32",  { name: "Float",  cType: "float",     default: "0.0f", color: [0x5B, 0x9B, 0xF0], size: 4  });
valueType("vec3", { name: "Vec3",   cType: "fm_vec3_t", default: "{}",   color: [0xB1, 0x6C, 0xE8], size: 12 });
valueType("quat", { name: "Quat",   cType: "fm_quat_t", default: "{}",   color: [0xE0, 0x72, 0xC8], size: 16 });

// Own type so it can't be wired into arithmetic, despite being an integer id.
valueType("objref", { name: "Object", cType: "uint16_t", default: "0", color: [0xE6, 0x9A, 0x3C], size: 4 });

// Implicit numeric conversions
convert("u32", "i32", "(int32_t)({})");
convert("u32", "f32", "(float)({})");
convert("i32", "u32", "(uint32_t)({})");
convert("i32", "f32", "(float)({})");
convert("f32", "u32", "(uint32_t)({})");
convert("f32", "i32", "(int32_t)({})");

// Scalar broadcast to all vec3 components.
convert("f32", "vec3", "fm_vec3_t{{}, {}, {}}");
convert("i32", "vec3", "fm_vec3_t{(float)({}), (float)({}), (float)({})}");
convert("u32", "vec3", "fm_vec3_t{(float)({}), (float)({}), (float)({})}");
