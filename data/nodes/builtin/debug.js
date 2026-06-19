// Logging nodes (one per data type), printing a value via P64::Log::info.

// Escape for a C++ string literal that is also a printf format.
function _esc(s) {
  return ("" + s).replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/%/g, "%%");
}

function _logNode(id, name, type, cType, fmt, argsOf) {
  node({
    id: id,
    name: icon("bug-outline") + " " + name,
    color: typeColor(type),
    rounding: 4.0,
    category: "Debug",
    inputs: [logicIn(), valueIn("", type)],
    outputs: [logicOut()],
    props: { label: Str({ label: "Label", width: 110 }) },
    build(n, ctx) {
      const label = _esc(n.label || "");
      if (ctx.hasValueInput(0)) {
        const pre = label ? label + " " : "";
        ctx.localVar(cType, "t_log", ctx.inputExpr(0))
           .line('P64::Log::info("' + pre + fmt + '", ' + argsOf("t_log") + ');');
      } else {
        ctx.line('P64::Log::info("' + label + '");');
      }
    },
  });
}

const _scalar = (v) => v;

_logNode("core.logInt",   "Log Int",    "i32",    "int32_t",   "%d", _scalar);
_logNode("core.logUInt",  "Log UInt",   "u32",    "uint32_t",  "%u", _scalar);
_logNode("core.logFloat", "Log Float",  "f32",    "float",     "%f", _scalar);
_logNode("core.logObj",   "Log Object", "objref", "uint16_t",  "%u", _scalar);
_logNode("core.logVec3",  "Log Vec3",   "vec3",   "fm_vec3_t", "%f %f %f",
         (v) => v + ".x, " + v + ".y, " + v + ".z");
_logNode("core.logQuat",  "Log Quat",   "quat",   "fm_quat_t", "%f %f %f %f",
         (v) => v + ".x, " + v + ".y, " + v + ".z, " + v + ".w");
