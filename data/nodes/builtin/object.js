// Object / scene / function builtin nodes.

node({
  id: "core.objDel",
  name: icon("trash-can-outline") + " Delete Object",
  color: Color.red,
  category: "Object",
  inputs: [logicIn(), valueIn("Object", "objref")],
  outputs: [logicOut()],
  build(n, ctx) {
    if (!ctx.hasValueInput(0)) {
      ctx.line("inst->object->remove();"); // delete self
    } else {
      ctx.localVar("uint16_t", "t_objId", ctx.inputExpr(0))
         .line("auto* t_obj = t_objId == 0 ? inst->object : inst->object->getScene().getObjectById(t_objId);")
         .line("if(t_obj) t_obj->remove();");
    }
  },
});

node({
  id: "core.objEvent",
  name: icon("email-fast-outline") + " Send Event",
  color: Color.green,
  category: "Object",
  inputs: [logicIn(), valueIn("Object", "objref")],
  outputs: [logicOut()],
  props: {
    eventType: U16({ label: "Type", width: 60 }),
    eventValue: Str({ label: "Value", default: "0", width: 80 }),
  },
  build(n, ctx) {
    ctx.localVar("uint16_t", "t_objId", ctx.inputExpr(0))
       .localConst("uint16_t", "t_eventType", n.eventType)
       .localConst("uint32_t", "t_eventVal", numberOrHash(n.eventValue))
       .line("inst->object->getScene().sendEvent(")
       .line("  t_objId == 0 ? inst->object->id : t_objId,")
       .line("  inst->object->id,")
       .line("  t_eventType,")
       .line("  t_eventVal")
       .line(");");
  },
});

node({
  // Deprecated in favour of an "objref" variable + Get Var; hidden but still loads.
  id: "core.objRef",
  name: icon("cube-outline") + " Object",
  hidden: true,
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  outputs: [valueOut("objref")],
  props: {
    objRefName: Str({ default: "Object", width: 120 }),
    objRefSlot: U16({ label: "Slot", width: 50 }),
  },
  build(n, ctx) { ctx.globalVar("uint16_t", n.res(), "inst->objRefs[" + n.objRefSlot + "]"); },
});

node({
  id: "core.sceneLoad",
  name: icon("earth-box") + " Load Scene",
  color: Color.green,
  category: "Scene",
  inputs: [logicIn(), valueIn("", "u32")],
  outputs: [logicOut()],
  props: { sceneId: Scene({ width: 110, hideIfInputConnected: 1 }) },
  build(n, ctx) {
    (ctx.hasValueInput(0)
      ? ctx.localVar("uint16_t", "sceneId", ctx.inputExpr(0))
      : ctx.localConst("uint16_t", "sceneId", n.sceneId))
      .line("P64::SceneManager::load(sceneId);");
  },
});

// Transform of the graph's own object (inst->object).
node({
  id: "core.objGetPos",
  name: icon("axis-arrow") + " Get Position",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  outputs: [valueOut("vec3")],
  value(n, ctx) { return "inst->object->pos"; },
});

// Transform captured when the graph starts. A shared graph-global (declareVar is keyed
// by name) seeded once with the object's pose, so all such nodes read the same value.
node({
  id: "core.objOrigPos",
  name: icon("axis-arrow") + " Original Position",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  outputs: [valueOut("vec3")],
  value(n, ctx) {
    ctx.declareVar("fm_vec3_t", "t_origPos", "inst->object->pos");
    return "t_origPos";
  },
});

node({
  id: "core.objOrigRot",
  name: icon("rotate-3d-variant") + " Original Rotation",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  outputs: [valueOut("quat")],
  value(n, ctx) {
    ctx.declareVar("fm_quat_t", "t_origRot", "inst->object->rot");
    return "t_origRot";
  },
});

// Op enum index -> assignment operator.
var _ASSIGN_OPS = ["= ", "+= ", "-= "];
function _opEnum() {
  return Enum({ values: ["Set", "Add", "Subtract"], label: "Op", width: 95 });
}
function _rotOpEnum() {
  return Enum({ values: ["Set", "Add"], label: "Op", width: 95 });
}
// Set, or compose + renormalize (to avoid drift), the object's rotation by a quat expr.
function _setRot(ctx, op, q) {
  if (op === 0) {
    ctx.line("inst->object->rot = " + q + ";");
  } else {
    ctx.line("fm_quat_t t_rot = (" + q + ") * inst->object->rot;");
    ctx.line("fm_quat_norm(&inst->object->rot, &t_rot);");
  }
}

node({
  id: "core.objSetPos",
  name: icon("axis-arrow") + " Set Position",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  inputs: [logicIn(), valueIn("", "vec3")],
  outputs: [logicOut()],
  props: { op: _opEnum() },
  title: "{op} Position",
  build(n, ctx) { ctx.line("inst->object->pos " + _ASSIGN_OPS[n.op.index] + ctx.inputExpr(0) + ";"); },
});

node({
  id: "core.objGetRot",
  name: icon("rotate-3d-variant") + " Get Rotation",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  outputs: [valueOut("quat")],
  value(n, ctx) { return "inst->object->rot"; },
});

node({
  id: "core.objSetRot",
  name: icon("rotate-3d-variant") + " Set Rotation",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  inputs: [logicIn(), valueIn("", "quat")],
  outputs: [logicOut()],
  props: { op: _rotOpEnum() },
  title: "{op} Rotation",
  build(n, ctx) {
    _setRot(ctx, n.op.index, ctx.inputExpr(0));
  },
});

node({
  id: "core.objSetRotEuler",
  name: icon("rotate-3d-variant") + " Set Rotation Euler",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  inputs: [logicIn(), valueIn("X", "f32"), valueIn("Y", "f32"), valueIn("Z", "f32")],
  outputs: [logicOut()],
  props: { op: _rotOpEnum() },
  title: "{op} Euler ZYX",
  build(n, ctx) {
    ctx.line("fm_quat_t t_e; fm_quat_from_euler_zyx(&t_e, " +
             ctx.inputExpr(0) + ", " + ctx.inputExpr(1) + ", " + ctx.inputExpr(2) + ");");
    _setRot(ctx, n.op.index, "t_e");
  },
});

node({
  id: "core.objSetRotEulerVec",
  name: icon("rotate-3d-variant") + " Set Rot Euler-Vector",
  color: Color.orange,
  rounding: 4.0,
  category: "Object",
  inputs: [logicIn(), valueIn("XYZ", "vec3")],
  outputs: [logicOut()],
  props: { op: _rotOpEnum() },
  title: "{op} Euler (ZYX)",
  build(n, ctx) {
    ctx.line("fm_quat_t t_e;");
    ctx.line("fm_vec3_t tmp = " + ctx.inputExpr(0) + ";");
    ctx.line("fm_quat_from_euler_zyx(&t_e, tmp.x, tmp.y, tmp.z);");
    _setRot(ctx, n.op.index, "t_e");
  },
});

node({
  id: "core.func",
  name: icon("function") + " Function",
  color: Color.green,
  category: "Logic",
  inputs: [logicIn()],
  outputs: [logicOut(), valueOut("i32")],
  props: { funcName: Str({ width: 80 }), arg0: Str({ default: "0", width: 80 }) },
  title: icon("function") + " {funcName}",
  build(n, ctx) {
    const funcVar = ctx.globalVar("UserFunc", "P64::NodeGraph::getFunction(" + crc32(n.funcName) + ")");
    ctx.globalVar("int", n.res(), 0)
       .localConst("uint32_t", "t_arg", numberOrHash(n.arg0))
       .line(n.res() + " = " + funcVar + "(t_arg);");
  },
});
