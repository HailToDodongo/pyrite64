console.log("Hello!");

editor.addAction("test.123", "Test JS Action", () => {
   console.log("Hello from Action 2!");
   editor.save();
});

editor.addAction("test.abc", "efefewffeffe", () => {
   console.log("Hello from Action 2!");
   editor.close();
});


