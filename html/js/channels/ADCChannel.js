(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  const adc_types = {
    "raw": "Raw Output",
    "digital_switch": "Digital Switching",
    "thermistor": "Thermistor",
    "4-20ma": "4-20mA Sensor",
    "high_volt_divider": "0-32v Input",
    "low_volt_divider": "0-5v Input",
    "ten_k_pullup": "10k Pullup"
  };

  let adc_running_averages = {};

  const ADCControlRow = (id, name, type) => `
<tr id="adc${id}" class="adcRow">
  <td class="adcId align-middle">${id}</td>
  <td class="adcName align-middle">${name}</td>
  <td class="adcType align-middle">${adc_types[type]}</td>
  <td class="adcValue" id="adcValue${id}"></td>
  <!-- <td class="adcBar align-middle">
     <div id="adcBar${id}" class="progress" role="progressbar" aria-label="ADC ${id} Reading" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100">
      <div class="progress-bar" style="width: 0%"></div>
    </div>
  </td> -->
</tr>
`;

  const ADCCalibrationTableRow = (ch, output, calibrated, index) => {
    return `
      <tr id="fADCCalibrationTableRow${ch.id}_${index}">
        <td>
          <div class="input-group input-group-sm">
            <input type="text" class="form-control" id="fADCCalibrationTableOutput${ch.id}_${index}" value="${output}" disabled>
            <span class="input-group-text ADCUnits${ch.id}">${ch.units}</span>
          </div>
        </td>
        <td>
          <div class="input-group input-group-sm">
            <input type="text" class="form-control" id="fADCCalibrationTableCalibrated${ch.id}_${index}" value="${calibrated}" disabled>
            <span class="input-group-text ADCCalibratedUnits${ch.id}">${ch.calibratedUnits}</span>
          </div>
        </td>
        <td>
          <button type="button" class="btn btn-sm btn-outline-danger" id="fADCCalibrationTableRemove${ch.id}_${index}">
            <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-trash" viewBox="0 0 16 16">
  <path d="M5.5 5.5A.5.5 0 0 1 6 6v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5m2.5 0a.5.5 0 0 1 .5.5v6a.5.5 0 0 1-1 0V6a.5.5 0 0 1 .5-.5m3 .5a.5.5 0 0 0-1 0v6a.5.5 0 0 0 1 0z"></path>
  <path d="M14.5 3a1 1 0 0 1-1 1H13v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V4h-.5a1 1 0 0 1-1-1V2a1 1 0 0 1 1-1H6a1 1 0 0 1 1-1h2a1 1 0 0 1 1 1h3.5a1 1 0 0 1 1 1zM4.118 4 4 4.059V13a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1V4.059L11.882 4zM2.5 3h11V2h-11z"></path>
  </svg>
          </button>
        </td>
      </tr>
    `;
  };

  const ADCEditCard = (ch) => {
    // Build options from adc_types
    const options = Object.entries(adc_types)
      .map(([key, label]) => `<option value="${key}">${label}</option>`)
      .join("\n");

    let calibrationTableHtml = "";
    if (YB.App.config.adc[ch.id - 1].hasOwnProperty("calibrationTable")) {
      calibrationTableHtml = YB.App.config.adc[ch.id - 1].calibrationTable
        .map(([output, calibrated], index) => ADCCalibrationTableRow(ch, output, calibrated, index))
        .join("\n");
    }

    return `
    <div id="adcEditCard${ch.id}" class="col-xs-12 col-sm-6">
      <div class="p-3 border border-secondary rounded">
        <h5>ADC #${ch.id}</h5>
        <div class="form-floating mb-3">
          <input type="text" class="form-control" id="fADCName${ch.id}" value="${ch.name}">
          <label for="fADCName${ch.id}">Name</label>
          <div class="invalid-feedback">Must be 30 characters or less.</div>
        </div>
        <div class="form-check form-switch mb-3">
          <input class="form-check-input" type="checkbox" id="fADCEnabled${ch.id}">
          <label class="form-check-label" for="fADCEnabled${ch.id}">Enabled</label>
        </div>
        <div class="form-floating mb-3">
          <select id="fADCType${ch.id}" class="form-select" aria-label="Input Type">
            ${options}
          </select>
          <label for="fADCType${ch.id}">Input Type</label>
        </div>
        <div class="form-floating mb-3">
          <select id="fADCDisplayDecimals${ch.id}" class="form-select" aria-label="Display Format">
            <option value="0">123,456</option>
            <option value="1">123,456.1</option>
            <option value="2">123,456.12</option>
            <option value="3">123,456.123</option>
            <option value="4">123,456.1234</option>
          </select>
          <label for="fADCDisplayDecimals${ch.id}">Display Format</label>
        </div>
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" id="fADCUseCalibrationTable${ch.id}">
          <label class="form-check-label" for="fADCUseCalibrationTable${ch.id}">Use Calibration Table</label>
        </div>
        <div id="fADCCalibrationTableUI${ch.id}" class="form-floating mt-3" style="display: none">
          <h5>Calibration Setup</h5>
          <div class="form-floating mb-3">
            <input type="text" class="form-control" id="fADCCalibratedUnits${ch.id}" value="${ch.calibratedUnits}">
            <label for="fADCCalibratedUnits${ch.id}">Calibrated Units</label>
            <div class="invalid-feedback">Must be 10 characters or less.</div>
          </div>
          <div class="form-floating mb-3">
            <h6>Live Averaged Output <span id="fADCAverageOutputCount${ch.id}" class="small"></span></h6>
            <div class="input-group">
              <input id="fADCAverageOutput${ch.id}" type="text" class="form-control" value="0">
              <span class="input-group-text ADCUnits${ch.id}">${ch.units}</span>
              <button id="fADCAverageOutputCopy${ch.id}" type="button" class="btn btn-sm btn-primary">
                Copy
              </button>
              <button id="fADCAverageOutputReset${ch.id}" type="button" class="btn btn-sm btn-secondary">
                Reset
              </button>
            </div>
          </div>
          <table class="table table-sm">
            <thead>
              <tr>
                <th>Output</th>
                <th>Calibrated</th>
                <th class="text-center"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-gear" viewBox="0 0 16 16">
  <path d="M8 4.754a3.246 3.246 0 1 0 0 6.492 3.246 3.246 0 0 0 0-6.492M5.754 8a2.246 2.246 0 1 1 4.492 0 2.246 2.246 0 0 1-4.492 0"/>
  <path d="M9.796 1.343c-.527-1.79-3.065-1.79-3.592 0l-.094.319a.873.873 0 0 1-1.255.52l-.292-.16c-1.64-.892-3.433.902-2.54 2.541l.159.292a.873.873 0 0 1-.52 1.255l-.319.094c-1.79.527-1.79 3.065 0 3.592l.319.094a.873.873 0 0 1 .52 1.255l-.16.292c-.892 1.64.901 3.434 2.541 2.54l.292-.159a.873.873 0 0 1 1.255.52l.094.319c.527 1.79 3.065 1.79 3.592 0l.094-.319a.873.873 0 0 1 1.255-.52l.292.16c1.64.893 3.434-.902 2.54-2.541l-.159-.292a.873.873 0 0 1 .52-1.255l.319-.094c1.79-.527 1.79-3.065 0-3.592l-.319-.094a.873.873 0 0 1-.52-1.255l.16-.292c.893-1.64-.902-3.433-2.541-2.54l-.292.159a.873.873 0 0 1-1.255-.52zm-2.633.283c.246-.835 1.428-.835 1.674 0l.094.319a1.873 1.873 0 0 0 2.693 1.115l.291-.16c.764-.415 1.6.42 1.184 1.185l-.159.292a1.873 1.873 0 0 0 1.116 2.692l.318.094c.835.246.835 1.428 0 1.674l-.319.094a1.873 1.873 0 0 0-1.115 2.693l.16.291c.415.764-.42 1.6-1.185 1.184l-.291-.159a1.873 1.873 0 0 0-2.693 1.116l-.094.318c-.246.835-1.428.835-1.674 0l-.094-.319a1.873 1.873 0 0 0-2.692-1.115l-.292.16c-.764.415-1.6-.42-1.184-1.185l.159-.291A1.873 1.873 0 0 0 1.945 8.93l-.319-.094c-.835-.246-.835-1.428 0-1.674l.319-.094A1.873 1.873 0 0 0 3.06 4.377l-.16-.292c-.415-.764.42-1.6 1.185-1.184l.292.159a1.873 1.873 0 0 0 2.692-1.115z"/>
</svg></th>
              </tr>
            </thead>
            <tbody id="ADCCalibrationTableBody${ch.id}">
              <tr>
                <td>
                  <div class="input-group input-group-sm">
                    <input type="text" class="form-control" id="fADCCalibrationTableOutput${ch.id}" value="">
                    <span class="input-group-text ADCUnits${ch.id}">${ch.units}</span>
                  </div>
                </td>
                <td>
                  <div class="input-group input-group-sm">
                    <input type="text" class="form-control" id="fADCCalibrationTableCalibrated${ch.id}" value="">
                    <span class="input-group-text ADCCalibratedUnits${ch.id}">${ch.calibratedUnits}</span>
                  </div>
                </td>
                <td>
                  <button id="fADCCalibrationTableAdd${ch.id}" type="button" class="btn btn-sm btn-outline-primary">
                    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-plus" viewBox="0 0 16 16">
  <path d="M8 4a.5.5 0 0 1 .5.5v3h3a.5.5 0 0 1 0 1h-3v3a.5.5 0 0 1-1 0v-3h-3a.5.5 0 0 1 0-1h3v-3A.5.5 0 0 1 8 4"/>
</svg>
                  </button>
                </td>
              </tr>
              ${calibrationTableHtml}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  `;
  };

  function adc_calibration_average_copy(e) {
    let ele = e.currentTarget;
    let id = ele.id.match(/\d+/)[0];

    let output = parseFloat($(`#fADCAverageOutput${id}`).val());
    $(`#fADCCalibrationTableOutput${id}`).val(output);
    $(`#fADCCalibrationTableCalibrated${id}`).focus();
  }

  function adc_calibration_average_reset(e) {
    let ele = e.currentTarget;
    let id = ele.id.match(/\d+/)[0];

    $(`#fADCAverageOutput${id}`).val(0.0);
    adc_running_averages[id] = [];
  }

  function validate_adc_add_calibration(e) {
    let ele = e.currentTarget;
    let id = ele.id.match(/\d+/)[0];

    let output = parseFloat($(`#fADCCalibrationTableOutput${id}`).val());
    let calibrated = parseFloat($(`#fADCCalibrationTableCalibrated${id}`).val());

    if (false) {
      $(ele).removeClass("is-valid");
      $(ele).addClass("is-invalid");
    }
    else {
      // $(ele).removeClass("is-invalid");
      // $(ele).addClass("is-valid");

      YB.client.send({
        "cmd": "config_adc",
        "id": id,
        "add_calibration": [output, calibrated]
      });

      //re-initialize
      $(`#fADCCalibrationTableOutput${id}`).val("");
      $(`#fADCCalibrationTableCalibrated${id}`).val("");

      //new row for the ui
      let index = 0;
      if (Array.isArray(YB.App.config.adc[id - 1].calibrationTable)) {
        index = YB.App.config.adc[id - 1].calibrationTable.length;
      }
      let newRow = ADCCalibrationTableRow(YB.App.config.adc[id], output, calibrated, index);
      $(`#ADCCalibrationTableBody${id}`).append(newRow);
      $(`#fADCCalibrationTableRemove${id}_${index}`).click(validate_adc_remove_calibration);
      YB.App.config.adc[id - 1].calibrationTable.push([output, calibrated]); //temporarily save it.
    }
  }

  function validate_adc_remove_calibration(e) {
    let ele = e.currentTarget;

    const match = ele.id.match(/(\d+)_(\d+)/);
    const id = parseInt(match[1], 10);
    const index = parseInt(match[2], 10);

    const output = parseFloat($(`#fADCCalibrationTableOutput${id}_${index}`).val());
    const calibrated = parseFloat($(`#fADCCalibrationTableCalibrated${id}_${index}`).val());

    if (false) {
      $(ele).removeClass("is-valid");
      $(ele).addClass("is-invalid");
    }
    else {
      // $(ele).removeClass("is-invalid");
      // $(ele).addClass("is-valid");

      YB.client.send({
        "cmd": "config_adc",
        "id": id,
        "remove_calibration": [output, calibrated]
      });

      //remove our row.
      $(`#fADCCalibrationTableRow${id}_${index}`).remove();
    }
  }

  function validate_adc_type(e) {
    let ele = e.target;
    let id = ele.id.match(/\d+/)[0];
    let value = ele.value;

    if (value.length <= 0 || value.length > 30) {
      $(ele).removeClass("is-valid");
      $(ele).addClass("is-invalid");
    }
    else {
      $(ele).removeClass("is-invalid");
      $(ele).addClass("is-valid");

      YB.client.send({
        "cmd": "config_adc",
        "id": id,
        "type": value
      });
    }
  }

  function validate_adc_display_decimals(e) {
    let ele = e.target;
    let id = ele.id.match(/\d+/)[0];
    let value = parseInt(ele.value);

    if (value.length <= 0 || value.length > 30) {
      $(ele).removeClass("is-valid");
      $(ele).addClass("is-invalid");
    }
    else {
      $(ele).removeClass("is-invalid");
      $(ele).addClass("is-valid");

      YB.client.send({
        "cmd": "config_adc",
        "id": id,
        "display_decimals": value
      });

      //update our current config.
      YB.App.config.adc[id - 1].displayDecimals = value;
    }
  }

  // ----- ADCChannel -----
  function ADCChannel() {
    YB.BaseChannel.call(this, "adc");
  }
  ADCChannel.prototype = Object.create(YB.BaseChannel.prototype);
  ADCChannel.prototype.constructor = ADCChannel;

  ADCChannel.prototype.getConfigSchema = function () {
    // copy base schema to avoid mutating the base literal
    var base = YB.BaseChannel.prototype.getConfigSchema.call(this);
    var schema = Object.assign({}, base);

    // Channel-specific fields
    schema.type = {
      presence: { allowEmpty: false },
      type: "string",
      length: { minimum: 1, maximum: 32 }
    };

    // Integer between 0–6 (display precision)
    schema.displayDecimals = {
      numericality: {
        onlyInteger: true,
        greaterThanOrEqualTo: 0,
        lessThanOrEqualTo: 4
      }
    };

    // Boolean flag for whether calibration is used
    schema.useCalibrationTable = {
      inclusion: {
        within: [true, false],
        message: "^useCalibrationTable must be boolean"
      }
    };

    // Optional calibrated units string
    schema.calibratedUnits = {
      type: "string",
      length: { minimum: 0, maximum: 7 }
    };

    // Optional calibration table — must be array if present
    schema.calibrationTable = {
      type: "array"
    };

    return schema;
  };

  // Export
  YB.ADCChannel = ADCChannel;

  YB.ChannelRegistry.registerChannelType("adc", YB.ADCChannel)

  // expose to global
  global.YB = YB;
})(this);
