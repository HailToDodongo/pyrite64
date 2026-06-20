// Value / arithmetic / comparison builtin nodes.

// "core.value" keeps its old id for backward compatibility.
function _flit(v) { var s = "" + v; if (!/[.eE]/.test(s)) s += ".0"; return s + "f"; }

node({
  id: "core.constRad",
  name: icon("numeric") + " Radians",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("f32")],
  props: { value: Float({ width: 50, label: "Degree" }) },
  value(n, ctx) { 
    return (n.value * (Math.PI / 180)).toFixed(4) + "f";
  }
});

node({
  id: "core.value",
  name: icon("numeric") + " Int",
  color: typeColor("i32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("i32")],
  props: { value: U16({ width: 50 }) },
  build(n, ctx) { ctx.globalVar("int32_t", n.res(), n.value); },
});

node({
  id: "core.valueUInt",
  name: icon("numeric") + " UInt",
  color: typeColor("u32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("u32")],
  props: { value: U32({ width: 50 }) },
  build(n, ctx) { ctx.globalVar("uint32_t", n.res(), n.value); },
});

node({
  id: "core.valueFloat",
  name: icon("decimal") + " Float",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("f32")],
  props: { value: Float({ width: 60 }) },
  build(n, ctx) { ctx.globalVar("float", n.res(), _flit(n.value)); },
});


node({
  id: "core.valueVec3",
  name: icon("axis-arrow") + " Vec3",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Value",
  inputs: [valueIn("X", "f32"), valueIn("Y", "f32"), valueIn("Z", "f32")],
  outputs: [valueOut("vec3")],
  value(n, ctx) {
    return "fm_vec3_t{" + ctx.inputExpr(0) + ", " + ctx.inputExpr(1) + ", " + ctx.inputExpr(2) + "}";
  },
});

node({
  id: "core.valueQuat",
  name: icon("axis-arrow") + " Quat",
  color: typeColor("quat"),
  rounding: 4.0,
  category: "Value",
  inputs: [valueIn("X", "f32"), valueIn("Y", "f32"), valueIn("Z", "f32"), valueIn("W", "f32", 1)],
  outputs: [valueOut("quat")],
  value(n, ctx) {
    return "fm_quat_t{" + ctx.inputExpr(0) + ", " + ctx.inputExpr(1) + ", " +
                          ctx.inputExpr(2) + ", " + ctx.inputExpr(3) + "}";
  },
});

node({
  id: "core.arg",
  name: icon("numeric") + " Argument",
  color: typeColor("u32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("u32")],
  props: { value: U16({ label: "Index", width: 50 }) },
  build(n, ctx) { ctx.globalVar("uint32_t&", n.res(), "inst->args[" + n.value + "]"); },
});

node({
  id: "core.deltaTime",
  name: icon("timer-outline") + " Delta Time",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Value",
  outputs: [valueOut("f32")],
  value(n, ctx) { return "P64::VI::SwapChain::getDeltaTime()"; },
});

// A single infix operator over two same-typed operands.
function _mathOp(id, ic, label, op, type, category) {
  node({
    id: id,
    name: icon(ic) + " " + label,
    color: typeColor(type),
    rounding: 4.0,
    category: category,
    inputs: [valueIn("A", type), valueIn("B", type)],
    outputs: [valueOut(type)],
    value(n, ctx) { return "(" + ctx.inputExpr(0) + " " + op + " " + ctx.inputExpr(1) + ")"; },
  });
}

_mathOp("core.add",  "plus",     "Add",      "+", "f32", "Math");
_mathOp("core.sub",  "minus",    "Subtract", "-", "f32", "Math");
_mathOp("core.mul",  "close",    "Multiply", "*", "f32", "Math");
_mathOp("core.div",  "division", "Divide",   "/", "f32", "Math");
_mathOp("core.imod", "percent",  "Int Mod",  "%", "i32", "Math");

node({
  id: "core.fmod",
  name: icon("percent") + " Float Mod",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Math",
  inputs: [valueIn("A", "f32"), valueIn("B", "f32")],
  outputs: [valueOut("f32")],
  value(n, ctx) { return "fmodf(" + ctx.inputExpr(0) + ", " + ctx.inputExpr(1) + ")"; },
});

node({
  id: "core.negate",
  name: icon("minus-box-outline") + " Negate",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Math",
  inputs: [valueIn("A", "f32")],
  outputs: [valueOut("f32")],
  value(n, ctx) { return "-" + ctx.inputExpr(0); },
});

_mathOp("core.vadd", "plus",     "Add",      "+", "vec3", "Vector Math");
_mathOp("core.vsub", "minus",    "Subtract", "-", "vec3", "Vector Math");
_mathOp("core.vmul", "close",    "Multiply", "*", "vec3", "Vector Math");
_mathOp("core.vdiv", "division", "Divide",   "/", "vec3", "Vector Math");

// Materialise a vec3 operand into a named temporary (for the out-param helpers below).
function _vtmp(expr, name) { return "fm_vec3_t " + name + " = " + expr + "; "; }

node({
  id: "core.vdot",
  name: icon("angle-acute") + " Dot",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("A", "vec3"), valueIn("B", "vec3")],
  outputs: [valueOut("f32")],
  value(n, ctx) {
    return lambda(_vtmp(ctx.inputExpr(0), "a") + _vtmp(ctx.inputExpr(1), "b") +
                  "return fm_vec3_dot(&a, &b);");
  },
});

node({
  id: "core.vcross",
  name: icon("close-octagon-outline") + " Cross",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("A", "vec3"), valueIn("B", "vec3")],
  outputs: [valueOut("vec3")],
  value(n, ctx) {
    return lambda(_vtmp(ctx.inputExpr(0), "a") + _vtmp(ctx.inputExpr(1), "b") +
                  "fm_vec3_t r; fm_vec3_cross(&r, &a, &b); return r;");
  },
});

node({
  id: "core.vlen",
  name: icon("ruler") + " Length",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("", "vec3")],
  outputs: [valueOut("f32")],
  value(n, ctx) { return lambda(_vtmp(ctx.inputExpr(0), "a") + "return fm_vec3_len(&a);"); },
});

node({
  id: "core.vdist",
  name: icon("ruler-square") + " Distance",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("A", "vec3"), valueIn("B", "vec3")],
  outputs: [valueOut("f32")],
  value(n, ctx) {
    return lambda(_vtmp(ctx.inputExpr(0), "a") + _vtmp(ctx.inputExpr(1), "b") +
                  "return fm_vec3_distance(&a, &b);");
  },
});

node({
  id: "core.vnorm",
  name: icon("vector-line") + " Normalize",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("", "vec3")],
  outputs: [valueOut("vec3")],
  value(n, ctx) {
    return lambda(_vtmp(ctx.inputExpr(0), "a") + "fm_vec3_t r; fm_vec3_norm(&r, &a); return r;");
  },
});


node({
  id: "core.vecSwap",
  name: icon("swap-vertical") + " Swap",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("", "vec3")],
  outputs: [valueOut("vec3")],
  props: { op: Enum(
    { values: ["ZXY", "YZX", "XZY", "Rand"],
      label: "Type", width: 60
    }
  )},
  value(n, ctx) {
    switch(n.op.value) {
      case "ZXY": return lambda(_vtmp(ctx.inputExpr(0), "a") + "return fm_vec3_t{a.z, a.x, a.y};");
      case "YZX": return lambda(_vtmp(ctx.inputExpr(0), "a") + "return fm_vec3_t{a.y, a.z, a.x};");
      case "XZY": return lambda(_vtmp(ctx.inputExpr(0), "a") + "return fm_vec3_t{a.x, a.z, a.y};");
      case "Rand": return lambda(_vtmp(ctx.inputExpr(0), "a") + `
        if(rand() & 1)std::swap(a.x, a.y);
        if(rand() & 1)std::swap(a.z, a.y);
        if(rand() & 1)std::swap(a.x, a.z);
        return a;
      `);
    }
  },
});
node({
  id: "core.vlerp",
  name: icon("vector-polyline") + " Lerp",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("A", "vec3"), valueIn("B", "vec3"), valueIn("t", "f32")],
  outputs: [valueOut("vec3")],
  value(n, ctx) {
    return lambda(_vtmp(ctx.inputExpr(0), "a") + _vtmp(ctx.inputExpr(1), "b") +
                  "fm_vec3_t r; fm_vec3_lerp(&r, &a, &b, " + ctx.inputExpr(2) + "); return r;");
  },
});

node({
  id: "core.vnegate",
  name: icon("minus-box-outline") + " Negate",
  color: typeColor("vec3"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("", "vec3")],
  outputs: [valueOut("vec3")],
  value(n, ctx) { return "(-(" + ctx.inputExpr(0) + "))"; },
});

node({
  id: "core.vcomp",
  name: icon("axis-arrow") + " Component",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Vector Math",
  inputs: [valueIn("", "vec3")],
  outputs: [valueOut("f32")],
  props: { axis: Enum({ values: ["x", "y", "z"], label: "Axis", width: 45 }) },
  title: "Component {axis}",
  value(n, ctx) { return "(" + ctx.inputExpr(0) + ")." + n.axis.value; },
});

// --- trigonometry (scalar + component-wise vector) ---------------------------
function _trigScalar(id, label, fn) {
  node({
    id: id,
    name: icon("sine-wave") + " " + label,
    color: typeColor("f32"),
    rounding: 4.0,
    category: "Math",
    inputs: [valueIn("", "f32")],
    outputs: [valueOut("f32")],
    value(n, ctx) { return fn + "(" + ctx.inputExpr(0) + ")"; }, // radians
  });
}
_trigScalar("core.sin", "Sin", "sinf");
_trigScalar("core.cos", "Cos", "cosf");
_trigScalar("core.tan", "Tan", "tanf");

function _trigVec(id, label, fn) {
  node({
    id: id,
    name: icon("sine-wave") + " " + label,
    color: typeColor("vec3"),
    rounding: 4.0,
    category: "Vector Math",
    inputs: [valueIn("", "vec3")],
    outputs: [valueOut("vec3")],
    value(n, ctx) {
      return lambda("fm_vec3_t a = " + ctx.inputExpr(0) +
                    "; return fm_vec3_t{" + fn + "(a.x), " + fn + "(a.y), " + fn + "(a.z)};");
    },
  });
}
_trigVec("core.vsin", "Sin", "sinf");
_trigVec("core.vcos", "Cos", "cosf");
_trigVec("core.vtan", "Tan", "tanf");

// --- easing: t in [0,1] -> [0,1] (input is clamped to that range) ------------
function _easeExpr(type) {
  switch (type) {
    case "Linear":      return "t";
    case "In Quad":     return "(t*t)";
    case "Out Quad":    return "(t*(2.0f-t))";
    case "InOut Quad":  return "(t<0.5f ? 2.0f*t*t : -1.0f+(4.0f-2.0f*t)*t)";
    case "In Cubic":    return "(t*t*t)";
    case "Out Cubic":   return "((t-1.0f)*(t-1.0f)*(t-1.0f)+1.0f)";
    case "InOut Cubic": return "(t<0.5f ? 4.0f*t*t*t : 1.0f-(-2.0f*t+2.0f)*(-2.0f*t+2.0f)*(-2.0f*t+2.0f)/2.0f)";
    case "In Sine":     return "(1.0f-cosf(t*1.5707963f))";
    case "Out Sine":    return "(sinf(t*1.5707963f))";
    case "InOut Sine":  return "(-(cosf(3.14159265f*t)-1.0f)/2.0f)";
    default:            return "t";
  }
}

node({
  id: "core.ease",
  name: icon("chart-bell-curve-cumulative") + " Ease",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Easing",
  inputs: [valueIn("in", "f32"), valueIn("scale", "f32", 1.0)],
  outputs: [valueOut("f32")],
  props: {
    ease: Enum({
      values: ["Linear", "In Quad", "Out Quad", "InOut Quad", "In Cubic", "Out Cubic",
               "InOut Cubic", "In Sine", "Out Sine", "InOut Sine"],
      label: "Type", width: 110,
    }),
  },
  title: "Ease {ease}",
  value(n, ctx) {
    return lambda(`
      float t = fminf(fmaxf(${ctx.inputExpr(0)}, 0.0f), 1.0f); 
      return ${_easeExpr(n.ease.value)} * ${ctx.inputExpr(1)};
    `);
  },
});

node({
  id: "core.mapRange",
  name: icon("arrow-expand-horizontal") + " Map Range",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Math",
  inputs: [valueIn("Value", "f32"), valueIn("From Min", "f32", 0), valueIn("From Max", "f32", 1),
           valueIn("To Min", "f32", 0), valueIn("To Max", "f32", 1)],
  outputs: [valueOut("f32")],
  value(n, ctx) {
    return lambda("float v=" + ctx.inputExpr(0) +
                  ", a=" + ctx.inputExpr(1) + ", b=" + ctx.inputExpr(2) +
                  ", c=" + ctx.inputExpr(3) + ", d=" + ctx.inputExpr(4) +
                  "; return c + (v - a) * (d - c) / (b - a);");
  },
});

// --- quaternion math ---------------------------------------------------------
node({
  id: "core.quatAxisAngle",
  name: icon("rotate-3d-variant") + " Rotate Axis",
  color: typeColor("quat"),
  rounding: 4.0,
  category: "Quat Math",
  inputs: [valueIn("Axis", "vec3"), valueIn("Angle", "f32")],
  outputs: [valueOut("quat")],
  value(n, ctx) {
    return lambda("fm_vec3_t ax = " + ctx.inputExpr(0) +
                  "; fm_quat_t q; fm_quat_from_axis_angle(&q, &ax, " + ctx.inputExpr(1) + "); return q;");
  },
});

node({
  id: "core.quatRotAxisZYX",
  name: icon("rotate-3d-variant") + " Rotate Quat ZYX",
  color: typeColor("quat"),
  rounding: 4.0,
  category: "Quat Math",
  inputs: [valueIn("Quat", "quat"), valueIn("X", "f32"), valueIn("Y", "f32"), valueIn("Z", "f32")],
  outputs: [valueOut("quat")],
  value(n, ctx) {
    return lambda(`
      fm_quat_t t_e;
      fm_quat_from_euler_zyx(&t_e, ${ctx.inputExpr(1)}, ${ctx.inputExpr(2)}, ${ctx.inputExpr(3)});
      fm_quat_t q = (${ctx.inputExpr(0)}) * t_e;
      fm_quat_norm(&q, &q);
      return q;
    `);
  },
});

node({
  id: "core.quatLerp",
  name: icon("vector-polyline") + " Quat Lerp",
  color: typeColor("quat"),
  rounding: 4.0,
  category: "Quat Math",
  inputs: [valueIn("A", "quat"), valueIn("B", "quat"), valueIn("t", "f32")],
  outputs: [valueOut("quat")],
  value(n, ctx) {
    return lambda("fm_quat_t a = " + ctx.inputExpr(0) + "; fm_quat_t b = " + ctx.inputExpr(1) +
                  "; fm_quat_t r; fm_quat_nlerp(&r, &a, &b, " + ctx.inputExpr(2) + "); return r;");
  },
});

// --- time-based waves --------------------------------------------------------
function _wave(id, label, ic, expr) {
  node({
    id: id,
    name: icon(ic) + " " + label,
    color: typeColor("f32"),
    rounding: 4.0,
    category: "Wave",
    inputs: [valueIn("Speed", "f32", 1), valueIn("Amplitude", "f32", 1), valueIn("Offset", "f32", 0)],
    outputs: [valueOut("f32")],
    props: { range: Enum({ values: ["-1..1", "0..1"], label: "Range", width: 70 }) },
    value(n, ctx) {
      // wave is in [-1,1]; "0..1" remaps it to a unipolar range
      var w = (n.range.index === 1) ? "((" + expr + ") * 0.5f + 0.5f)" : "(" + expr + ")";
      // phase in cycles
      return lambda("float p = inst->time * " + ctx.inputExpr(0) + " + " + ctx.inputExpr(2) +
                    "; return " + ctx.inputExpr(1) + " * " + w + ";");
    },
  });
}
_wave("core.waveSin",    "Sin Wave",    "sine-wave",     "fm_sinf(p * 6.2831853f)");
_wave("core.waveCos",    "Cos Wave",    "cosine-wave",   "fm_cosf(p * 6.2831853f)");
_wave("core.waveSquare", "Square Wave", "square-wave",   "((p - floorf(p)) < 0.5f ? 1.0f : -1.0f)");
_wave("core.waveSaw",    "Saw Wave",    "sawtooth-wave", "(2.0f * (p - floorf(p)) - 1.0f)");
_wave("core.waveTri",    "Triangle Wave", "triangle-wave", "(1.0f - 4.0f * fabsf((p + 0.25f) - floorf(p + 0.25f) - 0.5f))");
