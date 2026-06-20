node({
  id: "jam25.dialog",
  name: icon("message-text-outline") + " Dialog Message",
  color: [120, 120, 200],
  category: "Game",
  inputs:  [logicIn()],
  outputs: [logicOut(), valueOut("i32")],
  props: {
    name:  Str({ label: "Name" }),
  },
  build(n, ctx) {
    ctx.include("<user/systems/dialog.h>");
    ctx.globalVar("int32_t", n.res(), 0);
    ctx.setVar(n.res(), `User::Dialog::showMessage("${n.name}"_hash)`);
  },
});