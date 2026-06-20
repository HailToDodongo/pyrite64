node({
  id: "jam25.fadeIn",
  name: icon("overscan") + " Fade In",
  color: [120, 120, 200],
  category: "Game",
  inputs:  [logicIn(), valueIn("sec.", "f32")],
  outputs: [logicOut()],
  build(n, ctx) {
    ctx.include("<user/systems/screenFade.h>");
    ctx.line(`User::ScreenFade::fadeIn(0, ${ctx.inputExpr(0)});`);
  },
});

node({
  id: "jam25.fadeOut",
  name: icon("overscan") + " Fade Out",
  color: [120, 120, 200],
  category: "Game",
  inputs:  [logicIn(), valueIn("sec.", "f32")],
  outputs: [logicOut()],
  build(n, ctx) {
    ctx.include("<user/systems/screenFade.h>");
    ctx.line(`User::ScreenFade::fadeOut(0, ${ctx.inputExpr(0)});`);
  },
});

node({
  id: "jam25.setBars",
  name: icon("overscan") + " Set Bars",
  color: [120, 120, 200],
  category: "Game",
  inputs:  [logicIn(), valueIn("state", "i32")],
  outputs: [logicOut()],
  build(n, ctx) {
    ctx.include("<user/systems/context.h>");
    ctx.line(`User::ctx.forceBars = ${ctx.inputExpr(0)} != 0;`);
  },
});