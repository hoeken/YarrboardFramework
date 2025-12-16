(function (global) { //private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  function RelayChannel() {
    YB.BaseChannel.call(this, "relay", "Relay");

    // this.onEditForm = this.onEditForm.bind(this);
    this.toggleState = this.toggleState.bind(this);
  }

  RelayChannel.prototype = Object.create(YB.BaseChannel.prototype);
  RelayChannel.prototype.constructor = RelayChannel;

  RelayChannel.prototype.getConfigSchema = function () {
    // copy base schema to avoid mutating the base literal
    var base = YB.BaseChannel.prototype.getConfigSchema.call(this);

    var schema = Object.assign({}, base);

    schema.type = {
      presence: { allowEmpty: false },
      type: "string",
      length: { minimum: 1, maximum: 30 }
    };

    schema.defaultState = {
      presence: true,
      inclusion: {
        within: [true, false],
        message: "^defaultState must be boolean"
      }
    };

    return schema;
  };

  RelayChannel.prototype.generateControlUI = function () {
    return `
      <div id="relayControlCard${this.id}" class="col-xs-12 col-sm-6">
        <table class="w-100 h-100 p-2">
          <tr>
            <td>
              <button id="relayState${this.id}" type="button" class="btn relayButton text-center">
                <table style="width: 100%">
                  <tbody>
                    <tr>
                      <td class="relayIcon text-center align-middle pe-2">
                        ${YB.PWMChannel.typeImages[this.cfg.type]}
                      </td>
                      <td class="text-center" style="width: 99%">
                        <div id="relayName${this.id}">${this.name}</div>
                        <div id="relayStatus${this.id}"></div>
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

  RelayChannel.prototype.setupControlUI = function () {
    YB.BaseChannel.prototype.setupControlUI.call(this);

    $(`#relayState${this.id}`).on("click", this.toggleState);
  };

  RelayChannel.prototype.toggleState = function () {
    YB.client.send({
      "cmd": "toggle_relay_channel",
      "id": this.id,
      "source": YB.App.config.hostname
    }, true);
  }

  RelayChannel.prototype.generateEditUI = function () {

    let standardFields = this.generateStandardEditFields();

    return `
      <div id="relayEditCard${this.id}" class="col-xs-12 col-sm-6">
        <div class="p-3 border border-secondary rounded">
          <h5>Relay Channel #${this.id}</h5>
          ${standardFields}
          <div class="form-floating mb-3">
            <select id="f-relay-defaultState-${this.id}" class="form-select" aria-label="Default State (on boot)">
              <option value="ON">ON</option>
              <option value="OFF">OFF</option>
            </select>
            <label for="f-relay-defaultState-${this.id}">Default State (on boot)</label>
            <div class="invalid-feedback"></div>
          </div>
          <div class="form-floating">
            <select id="f-relay-type-${this.id}" class="form-select" aria-label="Output Type">
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
            <label for="f-relay-type-${this.id}">Output Type</label>
            <div class="invalid-feedback"></div>
          </div>
        </div>
      </div>
    `;
  };

  RelayChannel.prototype.setupEditUI = function () {

    YB.BaseChannel.prototype.setupEditUI.call(this);

    //populate our data
    $(`#f-relay-type-${this.id}`).val(this.cfg.type);
    $(`#f-relay-defaultState-${this.id}`).val(this.cfg.defaultState ? "ON" : "OFF");

    //enable/disable other stuff.
    $(`#f-relay-type-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-relay-defaultState-${this.id}`).prop('disabled', !this.enabled);

    //validate + save
    $(`#f-relay-type-${this.id}`).change(this.onEditForm);
    $(`#f-relay-defaultState-${this.id}`).change(this.onEditForm);
  };

  RelayChannel.prototype.getConfigFormData = function () {
    let newcfg = YB.BaseChannel.prototype.getConfigFormData.call(this);
    newcfg.type = $(`#f-relay-type-${this.id}`).val();
    newcfg.defaultState = $(`#f-relay-defaultState-${this.id}`).val() === "ON";
    return newcfg;
  };

  RelayChannel.prototype.onEditForm = function (e) {
    YB.BaseChannel.prototype.onEditForm.call(this, e);

    //ui updates
    $(`#f-relay-type-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-relay-defaultState-${this.id}`).prop('disabled', !this.enabled);
  };

  RelayChannel.prototype.updateControlUI = function () {
    if (this.data.state == "ON") {
      $('#relayState' + this.id).addClass("btn-success");
      $('#relayState' + this.id).removeClass("btn-secondary");
      $(`#relayStatus${this.id}`).hide();
    }
    else {
      $('#relayState' + this.id).addClass("btn-secondary");
      $('#relayState' + this.id).removeClass("btn-success");
      $(`#relayStatus${this.id}`).hide();
    }

    $(`#relayControlCard${this.id}`).toggle(this.enabled);
  };


  YB.RelayChannel = RelayChannel;
  YB.ChannelRegistry.registerChannelType("relay", YB.RelayChannel)

  global.YB = YB; // <-- this line makes it global

})(this); //private scope