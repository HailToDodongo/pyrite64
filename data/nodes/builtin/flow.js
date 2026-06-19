// Control-flow builtin nodes.

node({
  id: "core.start",
  name: icon("play") + " Start",
  color: Color.white,
  rounding: 4.0,
  entry: true,
  category: "Flow",
  outputs: [logicOut()],
  build(n, ctx) {},
});

node({
  id: "core.wait",
  name: icon("clock-outline") + " Wait",
  color: Color.green,
  category: "Flow",
  inputs: [logicIn()],
  outputs: [logicOut()],
  props: { time: Float({ label: "sec.", width: 50 }) },
  build(n, ctx) {
    ctx.localConst("uint64_t", "t_time", Math.floor(n.time * 1000))
       .line("coro_sleep(TICKS_FROM_MS(t_time));");
  },
});

node({
  id: "core.waitFrame",
  name: icon("debug-step-over") + " Wait Frame",
  color: Color.green,
  category: "Flow",
  inputs: [logicIn()],
  outputs: [logicOut()],
  build(n, ctx) {
    ctx.line("coro_yield();") // resume next frame
       .line("inst->time += P64::VI::SwapChain::getDeltaTime();"); // advance local time (frozen while waiting)
  },
});

node({
  id: "core.repeat",
  name: icon("repeat") + " Repeat",
  color: Color.green,
  category: "Flow",
  inputs: [logicIn()],
  outputs: [logicOut("Loop"), logicOut("Exit")],
  props: { count: U32({ width: 50 }) },
  build(n, ctx) {
    const counter = ctx.globalVar("uint8_t", 0);
    ctx.incrVar(counter, 1)
       .line("if(" + counter + " == " + n.count + ") {")
       .setVar(counter, 0)
       .jump(1)
       .line("}");
  },
});

node({
  id: "core.ifElse",
  name: icon("call-split") + " If-Else",
  color: Color.orange,
  rounding: 4.0,
  category: "Flow",
  inputs: [logicIn(), valueIn("", "i32")],
  outputs: [logicOut("True"), logicOut("False")],
  build(n, ctx) {
    ctx.localVar("int", "t_comp", ctx.inputExpr(0))
       .line("if(t_comp) {")
       .jump(0)
       .line("} else {")
       .jump(1)
       .line("}");
  },
});


node({
  id: "core.compare",
  name: icon("less-than-or-equal") + " Compare",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Flow",
  inputs: [logicIn(), valueIn("A", "f32"), valueIn("B", "f32")],
  outputs: [logicOut("True"), logicOut("False")],
  props: {
    compType: Enum({
      values: ["==", "!=", "<", "<=", ">", ">="],
      icons: ["equal", "not-equal", "less-than", "less-than-or-equal", "greater-than", "greater-than-or-equal"],
      label: "Oper.", width: 80,
    }),
  },
  title: "{compType.icon} Compare",
  build(n, ctx) {
    const a = ctx.inputExpr(0), b = ctx.inputExpr(1);
    ctx.localVar("int", "t_cmp", "(" + a + " " + n.compType.value + " " + b + ")")
      .line("if(t_cmp) {")
        .jump(0)
      .line("} else {")
        .jump(1)
      .line("}");
  },
});

// Fires the True path once each time the input value turns around (a peak or valley),
// i.e. when the sign of its change flips. Otherwise takes the False path.
node({
  id: "core.onExtremum",
  name: icon("chart-bell-curve") + " On Extremum",
  color: typeColor("f32"),
  rounding: 4.0,
  category: "Flow",
  inputs: [logicIn(), valueIn("Value", "f32")],
  outputs: [logicOut("True"), logicOut("False")],
  build(n, ctx) {
    const inited = ctx.globalVar("uint8_t", 0);
    const prevVal = ctx.globalVar("float", 0);
    const prevDir = ctx.globalVar("int8_t", 0); // -1/0/+1: last movement direction

    ctx.localVar("float", "t_v", ctx.inputExpr(0))
       .localVar("int8_t", "t_dir", prevDir)
       .line("if(" + inited + ") {")
       .line("  float t_d = t_v - " + prevVal + ";")
       .line("  if(t_d > 0.0f) t_dir = 1; else if(t_d < 0.0f) t_dir = -1;") // flat keeps last dir
       .line("}")
       .localVar("bool", "t_turn", "(" + prevDir + " != 0 && t_dir != 0 && t_dir != " + prevDir + ")")
       .setVar(prevVal, "t_v")
       .setVar(prevDir, "t_dir")
       .setVar(inited, 1)
       .line("if(t_turn) {")
       .jump(0)
       .line("} else {")
       .jump(1)
       .line("}");
  },
});