//use the onStart() call so we arent racing the app startup with document.onready
YB.App.onStart(function () {
  let home = YB.App.getPage("home");
  home.setContent(`
    <h1>Hello World</h1>
    <p>Edit js/test.js to modify the example</p>
  `);
});

//example page open callback
YB.App.getPage("stats").onOpen(function () {
  $("#statsContainer").append("<h1>Stats Page Opened</h1>");
});

//we arent using the config page.
YB.App.removePage("config");