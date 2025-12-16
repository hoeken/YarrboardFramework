(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  window.onerror = function (errorMsg, url, lineNumber) {
    YB.log(`Error: ${errorMsg} on ${url} line ${lineNumber}`);
    return false;
  }

  window.addEventListener("error", function (e) {
    YB.log(`Error Listener: ${e.error.message}`);
    YB.log(e.error.stack);
    return false;
  });

  window.addEventListener('unhandledrejection', function (e) {
    YB.log(`Unhandled Rejection: ${e.reason.message}`);
    return false;
  });

  YB.log = function (message) {

    //real messages only
    message = message.trim();
    if (!message.length)
      return;

    // Create a new Date object
    const currentDate = new Date();

    // Get the parts of the date and time
    const year = currentDate.getFullYear();
    const month = String(currentDate.getMonth() + 1).padStart(2, '0'); // Months are zero-based
    const day = String(currentDate.getDate()).padStart(2, '0');
    const hours = String(currentDate.getHours()).padStart(2, '0');
    const minutes = String(currentDate.getMinutes()).padStart(2, '0');
    const seconds = String(currentDate.getSeconds()).padStart(2, '0');
    const milliseconds = String(currentDate.getMilliseconds()).padStart(3, '0');

    // Create the formatted timestamp
    const formattedTimestamp = `[${year}-${month}-${day} ${hours}:${minutes}:${seconds}.${milliseconds}]`;

    //standard log.
    console.log(`${formattedTimestamp} ${message}`);

    //put it in our textarea - useful for debugging on non-computer interfaces
    let textarea = document.getElementById("debug_log_text");

    //append our message
    if (textarea.value)
      textarea.value += "\n";
    textarea.value += `${formattedTimestamp} ${message}`;

    //autoscroll
    if ($("#debug_log_autoscroll").is(':checked'))
      textarea.scrollTop = textarea.scrollHeight;
  };

  YB.log.consoleSendJSON = function (e) {
    e.preventDefault(); // stop real submission

    let text = $("#json_console_payload").val().trim();

    try {
      // Try to parse the JSON
      let obj = JSON.parse(text);

      // Clear the input if successful
      $("#json_console_payload").val("");

      YB.log("Sent: " + JSON.stringify(obj));

      // Send the parsed object (convert back to string if YB.client.send needs text)
      YB.client.send(obj);
    } catch (err) {
      YB.log("Invalid JSON: " + err);
    }
  }

  YB.log.setupDebugTerminal = function (e) {
    $("#json_console_form").on("submit", YB.log.consoleSendJSON);
    $("#json_console_send").on("click", YB.log.consoleSendJSON);

    $("#json_payload").on("keydown", function (e) {
      var key = e.key || e.keyCode || e.which;
      if (key === "Enter" || key === 13) {
        e.preventDefault();
        YB.log.consoleSendJSON();
      }
    });
  }

  // expose to global
  global.YB = YB;
})(this);