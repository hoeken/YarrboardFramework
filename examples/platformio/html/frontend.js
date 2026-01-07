//
// Yarrboard Framework Example
//
// This file gets automatically added to the end of the <body>
// of the HTML for the web interface.

const homepageMarkdown = `
# Hello World

You can edit **html/frontend.js** to modify this example.

The Yarrboard framework works on a JSON based command/message protocol.
This allows for a highly responsive web app that compiles to a single static HTML file for quick loading.

In this example code, we have defined a sample command called "test".  Here is the format for that command:

\`\`\`json
{"cmd":"test","foo":"bar"}
\`\`\`

There is a built-in Debug Console you can access by clicking on the copyright year in the footer.
You can use this to manually send commands.
`;

// Updating page content needs to use the onStart() call
// so we arent racing the app startup with document.onready
YB.App.onStart(function () {
  let home = YB.App.getPage("home");
  home.setContent(marked.parse(homepageMarkdown));
});

// Create a custom page
let customPage = new YB.Page({
  name: 'custom',
  displayName: 'Custom Page',
  permissionLevel: 'nobody',
  showInNavbar: true,
  position: "home",
  ready: true,
  content: '<h1>Custom Page</h1><p>This is our custom page.</p>'
});

// Add our open / close handlers and the page itself
customPage.onOpen(function () { console.log("Custom Page Opened.") });
customPage.onClose(function () { console.log("Custom Page Closed.") });
YB.App.addPage(customPage);

// Create a custom page
let customPanel = new YB.SettingsPanel({
  name: 'custom',
  displayName: 'Custom Settings',
  position: "general",
  content: `
    <div class="form-floating mb-3">
        <input id="custom_field" type="text" class="form-control">
        <label for="custom_field">Custom Field</label>
        <div class="invalid-feedback"></div>
    </div>
    <div class="text-center">
        <button id="saveCustomSettings" type="button" class="btn btn-primary">
            Save Custom Settings
        </button>
    </div>
  `
});
YB.App.addSettingsPanel(customPanel);

//click handlers and such need to be added after DOM is ready.
YB.App.onStart(function () {
  //click handler for our custom settings panel.
  $("#saveCustomSettings").on("click", function () {
    //pull our form data
    const settings = {
      custom_field: $("#custom_field").val().trim(),
    };

    //validate.js schema for our custom form data.
    const schema = {
      custom_field: {
        presence: { allowEmpty: false },
        length: { maximum: 31 },
      }
    }

    //validate it.
    const errors = validate(settings, schema);
    YB.Util.showFormValidationResults(settings, errors);

    //bail on fail.
    if (errors) {
      YB.Util.flashClass($("#customSettingsPanel"), "border-danger");
      YB.Util.flashClass($("#customGeneralSettings"), "btn-danger");
      return;
    }

    //log it for the user.
    YB.log(settings.custom_field);

    //flash whole form green.
    YB.Util.flashClass($("#customSettingsPanel"), "border-success");
    YB.Util.flashClass($("#customGeneralSettings"), "btn-success");

    //okay, send it off.
    YB.client.send({
      "cmd": "test",
      "foo": settings.custom_field
    });
  });
});

//Add some info text to the stats page.
const statsMarkdown = `
# Stats Page

This div is blank for you to modify with your own custom statistics and info.

The stats page automatically starts the get_stats poller, so you can add
a message handler for the stats message and then update the stats in realtime.
The frontend.js code has example handlers to dump the json of each message to the console.

Some of the rows on the **Board Information** table are expandable.  Try clicking on them to see more information.
`;

YB.App.onStart(function () {
  $("#statsContainer").append(marked.parse(statsMarkdown));
});

// Message handlers are called when a message with that name arrives
// update and stats are two standard ones which get polled on the
// home and stats page, respectively.
YB.App.onMessage("update", function (msg) { console.log(msg) });
YB.App.onMessage("stats", function (msg) { console.log(msg) });
YB.App.onMessage("test", function (msg) {
  YB.log(`"test" message handler: ${msg.bar}`);
  console.log(msg)
});