(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  function PWMChannel() {
    YB.BaseChannel.call(this, "pwm", "PWM");

    // this.onEditForm = this.onEditForm.bind(this);
    this.setDutyCycle = this.setDutyCycle.bind(this);
    this.toggleState = this.toggleState.bind(this);
  }

  PWMChannel.prototype = Object.create(YB.BaseChannel.prototype);
  PWMChannel.prototype.constructor = PWMChannel;

  PWMChannel.currentSliderID = -1;

  PWMChannel.typeImages = {
    "light":
      `<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="currentColor" class="bi bi-lightbulb" viewBox="0 0 16 16">
      <path d="M2 6a6 6 0 1 1 10.174 4.31c-.203.196-.359.4-.453.619l-.762 1.769A.5.5 0 0 1 10.5 13a.5.5 0 0 1 0 1 .5.5 0 0 1 0 1l-.224.447a1 1 0 0 1-.894.553H6.618a1 1 0 0 1-.894-.553L5.5 15a.5.5 0 0 1 0-1 .5.5 0 0 1 0-1 .5.5 0 0 1-.46-.302l-.761-1.77a1.964 1.964 0 0 0-.453-.618A5.984 5.984 0 0 1 2 6m6-5a5 5 0 0 0-3.479 8.592c.263.254.514.564.676.941L5.83 12h4.342l.632-1.467c.162-.377.413-.687.676-.941A5 5 0 0 0 8 1"/>
    </svg>`,
    "motor":
      `<svg xmlns="http://www.w3.org/2000/svg" height="32" width="32" version="1.1" fill="currentColor"
	 viewBox="0 0 452.263 452.263" xml:space="preserve">
      <path d="M405.02,161.533c-4.971,0-9,4.029-9,9v53.407h-18.298v-45.114c0-4.971-4.029-9-9-9h-41.756l-7.778-25.93
        c-1.142-3.807-4.646-6.414-8.621-6.414h-63.575v-22.77h40.032c4.971,0,9-4.029,9-9V75.666c0-4.971-4.029-9-9-9H158.064
        c-4.971,0-9,4.029-9,9v30.047c0,4.971,4.029,9,9,9h40.031v22.77h-92.714c-3.297,0-6.33,1.802-7.905,4.698l-15.044,27.646H47.227
        c-4.971,0-9,4.029-9,9v45.114H18v-64.342c0-4.971-4.029-9-9-9s-9,4.029-9,9v186.008c0,4.971,4.029,9,9,9s9-4.029,9-9v-64.342h20.227
        v47.64c0,4.971,4.029,9,9,9h74.936l34.26,44.207c1.705,2.2,4.331,3.487,7.114,3.487h205.185c4.971,0,9-4.029,9-9v-95.333h18.298
        v53.407c0,4.971,4.029,9,9,9c26.05,0,47.243-21.193,47.243-47.243v-87.651C452.263,182.727,431.07,161.533,405.02,161.533z
        M18,263.264V241.94h20.227v21.324H18z M167.064,84.666h110.959v12.047H167.064V84.666z M216.096,114.713h12.896v22.77h-12.896
        V114.713z M56.227,187.826h6.395v132.078h-6.395V187.826z M133.688,323.391c-1.705-2.2-4.331-3.487-7.114-3.487H80.622V187.826
        h7.159c3.297,0,6.33-1.802,7.905-4.698l15.044-27.646h193.14l7.778,25.93c1.142,3.807,4.646,6.414,8.621,6.414h16.151v179.771
        H167.948L133.688,323.391z M359.722,367.597h-5.302V187.826h5.302V367.597z M377.722,263.264V241.94h18.298v21.324H377.722z
        M434.263,296.428c0,12.986-8.508,24.022-20.243,27.827V180.95c11.735,3.804,20.243,14.84,20.243,27.827V296.428z"/>
      </svg>`,
    "water_pump":
      `<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="currentColor" class="bi bi-droplet" viewBox="0 0 16 16">
      <path fill-rule="evenodd" d="M7.21.8C7.69.295 8 0 8 0c.109.363.234.708.371 1.038.812 1.946 2.073 3.35 3.197 4.6C12.878 7.096 14 8.345 14 10a6 6 0 0 1-12 0C2 6.668 5.58 2.517 7.21.8zm.413 1.021A31.25 31.25 0 0 0 5.794 3.99c-.726.95-1.436 2.008-1.96 3.07C3.304 8.133 3 9.138 3 10a5 5 0 0 0 10 0c0-1.201-.796-2.157-2.181-3.7l-.03-.032C9.75 5.11 8.5 3.72 7.623 1.82z"/>
      <path fill-rule="evenodd" d="M4.553 7.776c.82-1.641 1.717-2.753 2.093-3.13l.708.708c-.29.29-1.128 1.311-1.907 2.87z"/>
    </svg>`,
    "bilge_pump":
      `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="32" height="32" fill="currentColor" version="1.1" x="0px" y="0px" viewBox="0 0 88.897 67.50625000000001" xml:space="preserve"><path d="M76.259,5.14l0.55-1.123c0.453-0.92,1.572-1.305,2.471-0.779l2.429,1.404  c0.854,0.494,1.154,1.609,0.718,2.486l-3.249,6.596l-5.552-3.207c-5.162-2.629-11.203-4.502-16.978-4.502H27.557  c-6.152,0-12.794,2.166-18.219,5.158l11.368,23.074h41.212l6.2-12.586l5.459,3.436L65.867,40.75  c-0.057,0.109-0.123,0.217-0.199,0.316l-0.608,1.227l-0.064,0.08c-0.271,0.338-0.745,0.566-1.163,0.656  c-0.972,0.215-2.003-0.121-2.861-0.566c-0.555-0.285-1.477-0.324-2.085-0.295c-1.333,0.061-2.698,0.426-3.935,0.906  c-4.462,1.727-8.49,5.334-11.597,8.885l-0.021,0.021l-2.022,2.025l-2.026-2.025l-0.021-0.021c-3.11-3.551-7.142-7.158-11.611-8.885  c-1.238-0.48-2.605-0.846-3.936-0.906c-0.613-0.029-1.535,0.01-2.093,0.295c-0.857,0.445-1.891,0.781-2.86,0.566  c-0.422-0.09-0.895-0.318-1.167-0.656l-0.063-0.078l-0.697-1.406c-0.045-0.064-0.026-0.035-0.074-0.139l-1.034-2.094l-1.562-3.15  l0.01-0.004L0.198,7.128c-0.435-0.881-0.136-1.992,0.72-2.486l2.429-1.404c0.897-0.525,2.014-0.141,2.47,0.779l0.873,1.777  C12.947,2.404,20.476,0,27.557,0h29.091C63.337,0,70.279,2.117,76.259,5.14L76.259,5.14z"/><polygon fill-rule="evenodd" clip-rule="evenodd" points="41.886,19.367 82.375,19.367 82.375,21.25 82.375,22.857   82.375,24.466 88.897,18.031 82.375,11.601 82.375,14.816 82.375,16.699 41.886,16.699 39.257,16.699 39.223,16.699 39.223,29.947   41.886,29.947 41.886,19.367 "/>`,
    "fuel_pump":
      `<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="currentColor" class="bi bi-fuel-pump" viewBox="0 0 16 16">
      <path d="M3 2.5a.5.5 0 0 1 .5-.5h5a.5.5 0 0 1 .5.5v5a.5.5 0 0 1-.5.5h-5a.5.5 0 0 1-.5-.5z"/>
      <path d="M1 2a2 2 0 0 1 2-2h6a2 2 0 0 1 2 2v8a2 2 0 0 1 2 2v.5a.5.5 0 0 0 1 0V8h-.5a.5.5 0 0 1-.5-.5V4.375a.5.5 0 0 1 .5-.5h1.495c-.011-.476-.053-.894-.201-1.222a.97.97 0 0 0-.394-.458c-.184-.11-.464-.195-.9-.195a.5.5 0 0 1 0-1c.564 0 1.034.11 1.412.336.383.228.634.551.794.907.295.655.294 1.465.294 2.081v3.175a.5.5 0 0 1-.5.501H15v4.5a1.5 1.5 0 0 1-3 0V12a1 1 0 0 0-1-1v4h.5a.5.5 0 0 1 0 1H.5a.5.5 0 0 1 0-1H1zm9 0a1 1 0 0 0-1-1H3a1 1 0 0 0-1 1v13h8z"/>
    </svg>`,
    "fan":
      `<svg width="32" height="32" fill="currentColor" id="Layer_1" data-name="Layer 1" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 122.88 122.07"><defs><style>.cls-1{fill-rule:evenodd;}</style></defs><title>fan-blades</title><path class="cls-1" d="M67.29,82.9c-.11,1.3-.26,2.6-.47,3.9-1.43,9-5.79,14.34-8.08,22.17C56,118.45,65.32,122.53,73.27,122A37.63,37.63,0,0,0,85,119a45,45,0,0,0,9.32-5.36c20.11-14.8,16-34.9-6.11-46.36a15,15,0,0,0-4.14-1.4,22,22,0,0,1-6,11.07l0,0A22.09,22.09,0,0,1,67.29,82.9ZM62.4,44.22a17.1,17.1,0,1,1-17.1,17.1,17.1,17.1,0,0,1,17.1-17.1ZM84.06,56.83c1.26.05,2.53.14,3.79.29,9.06,1,14.58,5.16,22.5,7.1,9.6,2.35,13.27-7.17,12.41-15.09a37.37,37.37,0,0,0-3.55-11.57,45.35,45.35,0,0,0-5.76-9.08C97.77,9,77.88,14,67.4,36.63a14.14,14.14,0,0,0-1,2.94A22,22,0,0,1,78,45.68l0,0a22.07,22.07,0,0,1,6,11.13Zm-26.9-17c0-1.6.13-3.21.31-4.81,1-9.07,5.12-14.6,7-22.52C66.86,2.89,57.32-.75,49.41.13A37.4,37.4,0,0,0,37.84,3.7a44.58,44.58,0,0,0-9.06,5.78C9.37,25.2,14.39,45.08,37,55.51a14.63,14.63,0,0,0,3.76,1.14A22.12,22.12,0,0,1,57.16,39.83ZM40.66,65.42a52.11,52.11,0,0,1-5.72-.24c-9.08-.88-14.67-4.92-22.62-6.73C2.68,56.25-.83,65.84.16,73.74A37.45,37.45,0,0,0,3.9,85.25a45.06,45.06,0,0,0,5.91,9c16,19.17,35.8,13.87,45.91-8.91a15.93,15.93,0,0,0,.88-2.66A22.15,22.15,0,0,1,40.66,65.42Z"/></svg>`,
    "solenoid":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-magnet" viewBox="0 0 16 16">
    <path d="M8 1a7 7 0 0 0-7 7v3h4V8a3 3 0 0 1 6 0v3h4V8a7 7 0 0 0-7-7m7 11h-4v3h4zM5 12H1v3h4zM0 8a8 8 0 1 1 16 0v8h-6V8a2 2 0 1 0-4 0v8H0z"/>
  </svg>`,
    "fridge":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-thermometer-low" viewBox="0 0 16 16">
    <path d="M9.5 12.5a1.5 1.5 0 1 1-2-1.415V9.5a.5.5 0 0 1 1 0v1.585a1.5 1.5 0 0 1 1 1.415"/>
    <path d="M5.5 2.5a2.5 2.5 0 0 1 5 0v7.55a3.5 3.5 0 1 1-5 0zM8 1a1.5 1.5 0 0 0-1.5 1.5v7.987l-.167.15a2.5 2.5 0 1 0 3.333 0l-.166-.15V2.5A1.5 1.5 0 0 0 8 1"/>
  </svg>`,
    "freezer":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-snow" viewBox="0 0 16 16">
      <path d="M8 16a.5.5 0 0 1-.5-.5v-1.293l-.646.647a.5.5 0 0 1-.707-.708L7.5 12.793V8.866l-3.4 1.963-.496 1.85a.5.5 0 1 1-.966-.26l.237-.882-1.12.646a.5.5 0 0 1-.5-.866l1.12-.646-.884-.237a.5.5 0 1 1 .26-.966l1.848.495L7 8 3.6 6.037l-1.85.495a.5.5 0 0 1-.258-.966l.883-.237-1.12-.646a.5.5 0 1 1 .5-.866l1.12.646-.237-.883a.5.5 0 1 1 .966-.258l.495 1.849L7.5 7.134V3.207L6.147 1.854a.5.5 0 1 1 .707-.708l.646.647V.5a.5.5 0 1 1 1 0v1.293l.647-.647a.5.5 0 1 1 .707.708L8.5 3.207v3.927l3.4-1.963.496-1.85a.5.5 0 1 1 .966.26l-.236.882 1.12-.646a.5.5 0 0 1 .5.866l-1.12.646.883.237a.5.5 0 1 1-.26.966l-1.848-.495L9 8l3.4 1.963 1.849-.495a.5.5 0 0 1 .259.966l-.883.237 1.12.646a.5.5 0 0 1-.5.866l-1.12-.646.236.883a.5.5 0 1 1-.966.258l-.495-1.849-3.4-1.963v3.927l1.353 1.353a.5.5 0 0 1-.707.708l-.647-.647V15.5a.5.5 0 0 1-.5.5z"/>
    </svg>`,
    "charger":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-lightning-charge" viewBox="0 0 16 16">
    <path d="M11.251.068a.5.5 0 0 1 .227.58L9.677 6.5H13a.5.5 0 0 1 .364.843l-8 8.5a.5.5 0 0 1-.842-.49L6.323 9.5H3a.5.5 0 0 1-.364-.843l8-8.5a.5.5 0 0 1 .615-.09zM4.157 8.5H7a.5.5 0 0 1 .478.647L6.11 13.59l5.732-6.09H9a.5.5 0 0 1-.478-.647L9.89 2.41z"/>
  </svg>`,
    "electronics":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-motherboard" viewBox="0 0 16 16">
    <path d="M11.5 2a.5.5 0 0 1 .5.5v7a.5.5 0 0 1-1 0v-7a.5.5 0 0 1 .5-.5m2 0a.5.5 0 0 1 .5.5v7a.5.5 0 0 1-1 0v-7a.5.5 0 0 1 .5-.5m-10 8a.5.5 0 0 0 0 1h6a.5.5 0 0 0 0-1zm0 2a.5.5 0 0 0 0 1h6a.5.5 0 0 0 0-1zM5 3a1 1 0 0 0-1 1h-.5a.5.5 0 0 0 0 1H4v1h-.5a.5.5 0 0 0 0 1H4a1 1 0 0 0 1 1v.5a.5.5 0 0 0 1 0V8h1v.5a.5.5 0 0 0 1 0V8a1 1 0 0 0 1-1h.5a.5.5 0 0 0 0-1H9V5h.5a.5.5 0 0 0 0-1H9a1 1 0 0 0-1-1v-.5a.5.5 0 0 0-1 0V3H6v-.5a.5.5 0 0 0-1 0zm0 1h3v3H5zm6.5 7a.5.5 0 0 0-.5.5v1a.5.5 0 0 0 .5.5h2a.5.5 0 0 0 .5-.5v-1a.5.5 0 0 0-.5-.5z"/>
    <path d="M1 2a2 2 0 0 1 2-2h11a2 2 0 0 1 2 2v11a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2v-2H.5a.5.5 0 0 1-.5-.5v-1A.5.5 0 0 1 .5 9H1V8H.5a.5.5 0 0 1-.5-.5v-1A.5.5 0 0 1 .5 6H1V5H.5a.5.5 0 0 1-.5-.5v-2A.5.5 0 0 1 .5 2zm1 11a1 1 0 0 0 1 1h11a1 1 0 0 0 1-1V2a1 1 0 0 0-1-1H3a1 1 0 0 0-1 1z"/>
  </svg>`,
    "other":
      `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-gear" viewBox="0 0 16 16">
      <path d="M8 4.754a3.246 3.246 0 1 0 0 6.492 3.246 3.246 0 0 0 0-6.492zM5.754 8a2.246 2.246 0 1 1 4.492 0 2.246 2.246 0 0 1-4.492 0z"/>
      <path d="M9.796 1.343c-.527-1.79-3.065-1.79-3.592 0l-.094.319a.873.873 0 0 1-1.255.52l-.292-.16c-1.64-.892-3.433.902-2.54 2.541l.159.292a.873.873 0 0 1-.52 1.255l-.319.094c-1.79.527-1.79 3.065 0 3.592l.319.094a.873.873 0 0 1 .52 1.255l-.16.292c-.892 1.64.901 3.434 2.541 2.54l.292-.159a.873.873 0 0 1 1.255.52l.094.319c.527 1.79 3.065 1.79 3.592 0l.094-.319a.873.873 0 0 1 1.255-.52l.292.16c1.64.893 3.434-.902 2.54-2.541l-.159-.292a.873.873 0 0 1 .52-1.255l.319-.094c1.79-.527 1.79-3.065 0-3.592l-.319-.094a.873.873 0 0 1-.52-1.255l.16-.292c.893-1.64-.902-3.433-2.541-2.54l-.292.159a.873.873 0 0 1-1.255-.52l-.094-.319zm-2.633.283c.246-.835 1.428-.835 1.674 0l.094.319a1.873 1.873 0 0 0 2.693 1.115l.291-.16c.764-.415 1.6.42 1.184 1.185l-.159.292a1.873 1.873 0 0 0 1.116 2.692l.318.094c.835.246.835 1.428 0 1.674l-.319.094a1.873 1.873 0 0 0-1.115 2.693l.16.291c.415.764-.42 1.6-1.185 1.184l-.291-.159a1.873 1.873 0 0 0-2.693 1.116l-.094.318c-.246.835-1.428.835-1.674 0l-.094-.319a1.873 1.873 0 0 0-2.692-1.115l-.292.16c-.764.415-1.6-.42-1.184-1.185l.159-.291A1.873 1.873 0 0 0 1.945 8.93l-.319-.094c-.835-.246-.835-1.428 0-1.674l.319-.094A1.873 1.873 0 0 0 3.06 4.377l-.16-.292c-.415-.764.42-1.6 1.185-1.184l.292.159a1.873 1.873 0 0 0 2.692-1.115l.094-.319z"/>
    </svg>`
  };

  PWMChannel.prototype.resetStats = function () {
    PWMChannel.total_ah = 0.0;
    PWMChannel.total_wh = 0.0;
    PWMChannel.total_on_count = 0;
    PWMChannel.total_trip_count = 0;
  };

  PWMChannel.prototype.generateStatsContainer = function () {
    return `
      <div id="pwmStatsDiv">
        <h5>PWM Statistics</h5>
        <table id="pwmStatsTable" class="table table-striped table-hover">
          <thead>
            <tr>
              <th scope="col">Channel</th>
              <th class="text-end" scope="col">aH</th>
              <th class="text-end" scope="col">wH</th>
              <th class="text-end" scope="col">Switches</th>
              <th class="text-end" scope="col">Trips</th>
            </tr>
          </thead>
          <tbody id="pwmStatsTableBody" class="table-group-divider"></tbody>
          <tfoot>
            <tr id="pwmStatsTotal"></tr>
              <th class="pwmName">Total</th>
              <th id="pwmAmpHoursTotal" class="text-end"></th>
              <th id="pwmWattHoursTotal" class="text-end"></th>
              <th id="pwmOnCountTotal" class="text-end"></th>
              <th id="pwmTripCountTotal" class="text-end"></th>
            </tr>
          </tfoot>
        </table>
      </div>
    `;
  };

  PWMChannel.prototype.getConfigSchema = function () {
    // copy base schema to avoid mutating the base literal
    var base = YB.BaseChannel.prototype.getConfigSchema.call(this);

    var schema = Object.assign({}, base);

    // Channel-specific fields
    schema.type = {
      presence: { allowEmpty: false },
      type: "string",
      length: { minimum: 1, maximum: 30 }
    };

    // Software Fuse
    schema.softFuse = {
      numericality: {
        onlyInteger: false,
        greaterThan: 0,
        lessThanOrEqualTo: 20
      }
    };

    // Boolean flag for whether calibration is used
    schema.isDimmable = {
      inclusion: {
        within: [true, false],
        message: "^isDimmable must be boolean"
      }
    };

    // Optional calibrated units string
    schema.defaultState = {
      type: "string",
      inclusion: {
        within: ["ON", "OFF"],
        message: "^defaultState must be ON or OFF"
      }
    };

    return schema;
  };

  PWMChannel.prototype.generateControlUI = function () {
    return `
      <div id="pwmControlCard${this.id}" class="col-xs-12 col-sm-6">
        <table class="w-100 h-100 p-2">
          <tr>
            <td>
              <button id="pwmState${this.id}" type="button" class="btn pwmButton text-center">
                <table style="width: 100%">
                  <tbody>
                    <tr>
                      <td id="pwmDutySliderControl${this.id}" class="pe-4" style="display: none">
                        <input class="pwmDutySlider" type="range" min="0" max="100" id="pwmDutySlider${this.id}" orient="vertical">
                      </td>
                      <td class="pwmIcon text-center align-middle pe-2">
                        ${PWMChannel.typeImages[this.cfg.type]}
                      </td>
                      <td class="text-center" style="width: 99%">
                        <div id="pwmName${this.id}">${this.name}</div>
                        <div id="pwmStatus${this.id}"></div>
                      </td>
                      <td>
                        <div id="pwmData${this.id}" class="text-end pwmData">
                          <div id="pwmVoltage${this.id}"></div>
                          <div id="pwmCurrent${this.id}"></div>
                          <div id="pwmWattage${this.id}"></div>
                        </div>
                      </td>
                    </tr>
                  </tbody>
                </table>
              </button>
            </td>
          </tr>
        </table>
      </div>
    `;
  };

  PWMChannel.prototype.setupControlUI = function () {
    YB.BaseChannel.prototype.setupControlUI.call(this);

    $(`#pwmState${this.id}`).on("click", this.toggleState);

    if (this.cfg.isDimmable) {
      if (!YB.App.isMFD())
        $(`#pwmDutySliderControl${this.id}`).show();

      //stop it from toggling
      $('#pwmDutySlider' + this.id).on("click", function (event) {
        event.preventDefault();
        event.stopPropagation();
      });

      $('#pwmDutySlider' + this.id).change(this.setDutyCycle);

      //update our duty when we move
      $('#pwmDutySlider' + this.id).on("input", this.setDutyCycle);

      //stop updating the UI when we are choosing a duty
      $('#pwmDutySlider' + this.id).on('focus', function (e) {
        let ele = e.target;
        let id = ele.id.match(/\d+/)[0];
        PWMChannel.currentSliderID = id;
      });

      //stop updating the UI when we are choosing a duty
      $('#pwmDutySlider' + this.id).on('touchstart', function (e) {
        let ele = e.target;
        let id = ele.id.match(/\d+/)[0];
        PWMChannel.currentSliderID = id;
      });

      //restart the UI updates when slider is closed
      $('#pwmDutySlider' + this.id).on("blur", function (e) {
        PWMChannel.currentSliderID = -1;
      });

      //restart the UI updates when slider is closed
      $('#pwmDutySlider' + this.id).on("touchend", function (e) {
        PWMChannel.currentSliderID = -1;
      });
    }
  };

  PWMChannel.prototype.generateEditUI = function () {

    let standardFields = this.generateStandardEditFields();

    return `
      <div id="pwmEditCard${this.id}" class="col-xs-12 col-sm-6">
        <div class="p-3 border border-secondary rounded">
          <h5>PWM Channel #${this.id}</h5>
          ${standardFields}
          <div class="form-check form-switch mb-3">
            <input class="form-check-input" type="checkbox" id="f-pwm-isDimmable-${this.id}">
            <label class="form-check-label" for="f-pwm-isDimmable-${this.id}">
              Dimmable?
            </label>
            <div class="invalid-feedback"></div>
          </div>
          <div class="form-floating mb-3">
            <input type="text" class="form-control" id="f-pwm-softFuse-${this.id}" value="${this.cfg.softFuse}">
            <label for="f-pwm-softFuse-${this.id}">Soft Fuse (Amps)</label>
            <div class="invalid-feedback"></div>
          </div>
          <div class="form-floating mb-3">
            <select id="f-pwm-defaultState-${this.id}" class="form-select" aria-label="Default State (on boot)">
              <option value="ON">ON</option>
              <option value="OFF">OFF</option>
            </select>
            <label for="f-pwm-defaultState-${this.id}">Default State (on boot)</label>
            <div class="invalid-feedback"></div>
          </div>
          <div class="form-floating">
            <select id="f-pwm-type-${this.id}" class="form-select" aria-label="Output Type">
              <option value="light">Light</option>
              <option value="motor">Motor</option>
              <option value="water_pump">Water Pump</option>
              <option value="bilge_pump">Bilge Pump</option>
              <option value="fuel_pump">Fuel Pump</option>
              <option value="fan">Fan</option>
              <option value="solenoid">Solenoid</option>
              <option value="fridge">Refrigerator</option>
              <option value="freezer">Freezer</option>
              <option value="charger">Charger</option>
              <option value="electronics">Electronics</option>
              <option value="other">Other</option>
            </select>
            <label for="f-pwm-type-${this.id}">Output Type</label>
            <div class="invalid-feedback"></div>
          </div>
        </div>
      </div>
    `;
  };

  PWMChannel.prototype.setupEditUI = function () {

    YB.BaseChannel.prototype.setupEditUI.call(this);

    $(`#f-pwm-isDimmable-${this.id}`).prop("checked", this.cfg.isDimmable);
    $(`#f-pwm-type-${this.id}`).val(this.cfg.type);
    $(`#f-pwm-defaultState-${this.id}`).val(this.cfg.defaultState);

    //enable/disable other stuff.
    $(`#f-pwm-isDimmable-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-softFuse-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-type-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-defaultState-${this.id}`).prop('disabled', !this.enabled);

    //validate + save
    $(`#f-pwm-isDimmable-${this.id}`).change(this.onEditForm);
    $(`#f-pwm-softFuse-${this.id}`).change(this.onEditForm);
    $(`#f-pwm-type-${this.id}`).change(this.onEditForm);
    $(`#f-pwm-defaultState-${this.id}`).change(this.onEditForm);
  };

  PWMChannel.prototype.getConfigFormData = function () {
    let newcfg = YB.BaseChannel.prototype.getConfigFormData.call(this);
    newcfg.isDimmable = $(`#f-pwm-isDimmable-${this.id}`).prop("checked");
    newcfg.softFuse = $(`#f-pwm-softFuse-${this.id}`).val();
    newcfg.type = $(`#f-pwm-type-${this.id}`).val();
    newcfg.defaultState = $(`#f-pwm-defaultState-${this.id}`).val();
    return newcfg;
  }

  PWMChannel.prototype.onEditForm = function (e) {
    YB.BaseChannel.prototype.onEditForm.call(this, e);

    //ui updates
    $(`#f-pwm-isDimmable-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-softFuse-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-type-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-pwm-defaultState-${this.id}`).prop('disabled', !this.enabled);
  };

  PWMChannel.prototype.setDutyCycle = function (e) {
    let ele = e.target;
    let value = ele.value;

    //must be realistic.
    if (value >= 0 && value <= 100) {
      //we want a duty value from 0 to 1
      value = value / 100;

      //update our duty cycle
      YB.client.setPWMChannelDuty(this.id, value, false);
    }

    e.stopPropagation();
    e.preventDefault();
  }

  PWMChannel.prototype.toggleState = function () {
    YB.client.togglePWMChannel(this.id, YB.App.config.hostname, true);
  }

  PWMChannel.prototype.updateControlUI = function () {
    if (this.data.state == "ON") {
      $('#pwmState' + this.id).addClass("btn-success");
      $('#pwmState' + this.id).removeClass("btn-primary");
      $('#pwmState' + this.id).removeClass("btn-warning");
      $('#pwmState' + this.id).removeClass("btn-danger");
      $('#pwmState' + this.id).removeClass("btn-secondary");
      $(`#pwmStatus${this.id}`).hide();
      $(`#pwmData${this.id}`).show();
    }
    else if (this.data.state == "TRIPPED") {
      $('#pwmState' + this.id).addClass("btn-warning");
      $('#pwmState' + this.id).removeClass("btn-primary");
      $('#pwmState' + this.id).removeClass("btn-success");
      $('#pwmState' + this.id).removeClass("btn-danger");
      $('#pwmState' + this.id).removeClass("btn-secondary");
      $(`#pwmStatus${this.id}`).html("SOFT TRIP");
      $(`#pwmStatus${this.id}`).show();
      $(`#pwmData${this.id}`).hide();
    }
    else if (this.data.state == "BLOWN") {
      $('#pwmState' + this.id).addClass("btn-danger");
      $('#pwmState' + this.id).removeClass("btn-primary");
      $('#pwmState' + this.id).removeClass("btn-warning");
      $('#pwmState' + this.id).removeClass("btn-success");
      $('#pwmState' + this.id).removeClass("btn-secondary");
      $(`#pwmStatus${this.id}`).html("FUSE BLOWN");
      $(`#pwmStatus${this.id}`).show();
      $(`#pwmData${this.id}`).hide();
    }
    else if (this.data.state == "BYPASSED") {
      $('#pwmState' + this.id).addClass("btn-primary");
      $('#pwmState' + this.id).removeClass("btn-danger");
      $('#pwmState' + this.id).removeClass("btn-warning");
      $('#pwmState' + this.id).removeClass("btn-success");
      $('#pwmState' + this.id).removeClass("btn-secondary");
      $(`#pwmStatus${this.id}`).html("BYPASSED");
      $(`#pwmStatus${this.id}`).show();
      $(`#pwmData${this.id}`).show();
    }
    else if (this.data.state == "OFF") {
      $('#pwmState' + this.id).addClass("btn-secondary");
      $('#pwmState' + this.id).removeClass("btn-primary");
      $('#pwmState' + this.id).removeClass("btn-warning");
      $('#pwmState' + this.id).removeClass("btn-success");
      $('#pwmState' + this.id).removeClass("btn-danger");
      $(`#pwmStatus${this.id}`).hide();
      $(`#pwmData${this.id}`).hide();
    }

    //duty is a bit of a special case.
    let duty = Math.round(this.data.duty * 100);
    if (this.cfg.isDimmable) {
      if (PWMChannel.currentSliderID != this.id) {
        $('#pwmDutySlider' + this.id).val(duty);
      }
    }

    let voltage = this.data.voltage.toFixed(1);
    $('#pwmVoltage' + this.id).html(`${voltage}V`);

    let current = this.data.current;
    if (current < 1)
      current = current.toFixed(2);
    else
      current = current.toFixed(1);
    $('#pwmCurrent' + this.id).html(`${current}A`);

    let wattage = this.data.wattage;
    if (wattage > 10)
      wattage = Math.round(wattage);
    else if (wattage > 1)
      wattage = wattage.toFixed(1);
    else
      wattage = wattage.toFixed(2);
    $('#pwmWattage' + this.id).html(`${wattage}W`);

    $(`#pwmControlCard${this.id}`).toggle(this.enabled);
  };

  PWMChannel.prototype.generateStatsUI = function () {
    $('#pwmStatsTableBody').append(`
      <tr id="pwmStats${this.id}">
        <td class="pwmName" > ${this.name}</td >
        <td id="pwmAmpHours${this.id}" class= "text-end"></td >
        <td id="pwmWattHours${this.id}" class= "text-end"></td >
        <td id="pwmOnCount${this.id}" class= "text-end"></td >
        <td id="pwmTripCount${this.id}" class= "text-end"></td >
      </tr>
    `);
  };

  PWMChannel.prototype.updateStatsUI = function (stats) {
    if (this.enabled) {
      $('#pwmAmpHours' + this.id).html(YB.Util.formatAmpHours(stats.aH));
      $('#pwmWattHours' + this.id).html(YB.Util.formatWattHours(stats.wH));
      $('#pwmOnCount' + this.id).html(stats.state_change_count.toLocaleString("en-US"));
      $('#pwmTripCount' + this.id).html(stats.soft_fuse_trip_count.toLocaleString("en-US"));

      PWMChannel.total_ah += parseFloat(stats.aH);
      PWMChannel.total_wh += parseFloat(stats.wH);
      PWMChannel.total_on_count += parseInt(stats.state_change_count);
      PWMChannel.total_trip_count += parseInt(stats.soft_fuse_trip_count);
    }

    $('#pwmAmpHoursTotal').html(YB.Util.formatAmpHours(PWMChannel.total_ah));
    $('#pwmWattHoursTotal').html(YB.Util.formatWattHours(PWMChannel.total_wh));
    $('#pwmOnCountTotal').html(PWMChannel.total_on_count.toLocaleString("en-US"));
    $('#pwmTripCountTotal').html(PWMChannel.total_trip_count.toLocaleString("en-US"));
  };

  YB.PWMChannel = PWMChannel;
  YB.ChannelRegistry.registerChannelType("pwm", YB.PWMChannel)

  // expose to global
  global.YB = YB;
})(this);