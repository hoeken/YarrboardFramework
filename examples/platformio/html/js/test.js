//set our control page html
YB.App.onStart(function () {
  let control = YB.App.getPage("control");
  control.setContent(`
    <h1>Hello World</h1>
    <p>Edit js/test.js to modify the example</p>
  `);
});

//example page open callback
YB.App.onPageOpen("stats", function () {
  console.log("Stats Page Opened");
  return true;
});