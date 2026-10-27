console.log("Hello!");

editor.addAction("test.123", "Test JS Action", () => {
   console.log("Hello from Action 2!");
   editor.save();
});

editor.addAction("user.avgHeight", "Average Height", () => {
   const objs = editor.getSelObjects();
   if(objs.length == 0)return;

   let h = 0;
   for(let o of objs)h += o.pos.y;
   h /= objs.length;

   for(let o of objs) {
    o.pos.y = h;
   }

   history.markChanged("Average Height");
});


