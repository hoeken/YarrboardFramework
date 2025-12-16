(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  // Base class for all channels
  function BaseChannel(type, name) {
    this.channelType = type;
    this.channelName = name;
    this.cfg = {};
    this.data = {};

    this.onEditForm = this.onEditForm.bind(this);
  }

  BaseChannel.prototype.generateControlContainer = function () {
    return `
      <div id="${this.channelType}ControlDiv" style="display:none" class="gy-3 col-md-12">
          <div id="${this.channelType}Cards" class="row g-3"></div>
      </div>
    `;
  };

  BaseChannel.prototype.generateEditContainer = function () {
    return `
      <div id="${this.channelType}Config" style="display:none" class="col-md-12">
        <h4>${this.channelName} Configuration</h4>
        <div id="${this.channelType}ConfigForm" class="row g-3"></div>
      </div>
    `;
  };


  BaseChannel.prototype.generateStandardEditFields = function () {
    return `
      <div class="form-floating mb-3">
        <input type="text" class="form-control" id="f-${this.channelType}-name-${this.id}" value="${this.name}">
        <label for="f-${this.channelType}-name-${this.id}">Name</label>
        <div class="invalid-feedback"></div>
      </div>
      <div class="form-floating mb-3">
        <input type="text" class="form-control" id="f-${this.channelType}-key-${this.id}" value="${this.key}">
        <label for="f-${this.channelType}-key-${this.id}">Key</label>
        <div class="invalid-feedback"></div>
      </div>
      <div class="form-check form-switch mb-3">
        <input class="form-check-input" type="checkbox" id="f-${this.channelType}-enabled-${this.id}">
        <label class="form-check-label" for="f-${this.channelType}-enabled-${this.id}">
          Enabled
        </label>
        <div class="invalid-feedback"></div>
      </div>
    `;
  }

  BaseChannel.prototype.resetStats = function () { };
  BaseChannel.prototype.generateStatsContainer = function () { };

  //
  // Schema for validation of the config object.
  //
  BaseChannel.prototype.getConfigSchema = function () {
    return {
      id: {
        presence: { allowEmpty: false },
        numericality: { onlyInteger: true, greaterThan: 0 }
      },
      key: {
        presence: { allowEmpty: false },
        type: "string",
        length: { minimum: 1, maximum: 63 },
        format: {
          pattern: /^[A-Za-z0-9_-]+$/,
          message: "can only contain letters, numbers, hyphens (-), or underscores (_)"
        }
      },
      name: {
        presence: { allowEmpty: false },
        type: "string",
        length: { minimum: 1, maximum: 63 }
      },
      enabled: {
        presence: true,
        inclusion: {
          within: [true, false],
          message: "^enabled must be boolean"
        }
      }
    };
  };

  //
  // Configuration settings for the channel itself
  //
  BaseChannel.prototype.getConfig = function () {
    return this.cfg;
  };

  BaseChannel.prototype.validateConfig = function (cfg) {
    return validate(cfg, this.getConfigSchema());
  };

  BaseChannel.prototype.loadConfig = function (cfg) {
    var errors = this.validateConfig(cfg);
    if (!errors) {
      // keep existing keys unless overwritten
      this.cfg = Object.assign({}, this.cfg, cfg);

      this.id = cfg.id;
      this.key = cfg.key;
      this.enabled = cfg.enabled;
      this.name = cfg.name;
    }
    return errors;
  };

  // validate and set a single config key
  BaseChannel.prototype.setConfigEntry = function (key, value) {
    var schema = this.getConfigSchema()[key];
    if (!schema) return { [key]: ["unknown config key"] };

    // validate.js has validate.single(value, constraints)
    var errors = validate.single(value, schema);
    if (!errors) this.cfg[key] = value;
    return errors;
  };

  BaseChannel.prototype.saveConfig = function () {
    var errors = this.validateConfig(this.getConfig());
    if (!errors) {
      let command = {
        cmd: `config_${this.channelType}_channel`,
        id: this.id,
        config: this.getConfig(),
      };
      if (YB.client && typeof YB.client.send === "function") {
        YB.client.send(command, true);
      } else {
        console.warn("[YB] client.send not available; skipped send", command);
      }
    }
    return errors;
  };

  //
  // Data holds the non-config things about the channel eg. state
  //
  BaseChannel.prototype.loadData = function (data) {
    this.data = data || {};
  };

  BaseChannel.prototype.getData = function () {
    return this.data;
  };

  //
  // Nitty gritty of form handling.
  //
  BaseChannel.prototype.handleEditForm = function (newcfg, event) {

    //clear our errors
    for (const f of Object.keys(newcfg)) {
      $(`#f-${this.channelType}-${f}-${this.id}`).removeClass("is-valid is-invalid");
    }

    //check for errors, and then save it.
    let errors = this.loadConfig(newcfg);
    if (errors) {
      for (const field of Object.keys(errors)) {

        //add our invalid class
        const $el = $(`#f-${this.channelType}-${field}-${this.id}`);
        $el.removeClass("is-valid").addClass("is-invalid");

        // Find the right feedback element (works for form-floating and form-switch)
        const msg = errors[field][0];
        const $fb = $el.siblings(".invalid-feedback")
          .add($el.closest(".form-check, .form-floating").find(".invalid-feedback"))
          .first();
        $fb.text(msg);
      }
    } else {
      $(event.target).addClass("is-valid");
      this.saveConfig();
      this.onConfigSuccess();
    }
  };

  BaseChannel.prototype.getConfigFormData = function () {
    // shallow copy so we don’t mutate this.cfg until save
    const newcfg = Object.assign({}, this.cfg);
    newcfg.name = $(`#f-${this.channelType}-name-${this.id}`).val();
    newcfg.enabled = $(`#f-${this.channelType}-enabled-${this.id}`).prop("checked");
    newcfg.key = $(`#f-${this.channelType}-key-${this.id}`).val();
    return newcfg;
  }

  BaseChannel.prototype.onEditForm = function (e) {

    //layer our form data over our existing config.
    let newcfg = this.getConfigFormData();

    this.handleEditForm(newcfg, e);

    $(`#f-${this.channelType}-name-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-${this.channelType}-key-${this.id}`).prop('disabled', !this.enabled);
  }

  BaseChannel.prototype.onConfigSuccess = function () {
    $(`#${this.channelType}ControlCard${this.id}`).replaceWith(this.generateControlUI());
    this.setupControlUI();
  };

  //
  // Functions for generating the html for various channel specific UI elements
  //
  BaseChannel.prototype.generateControlUI = function () { };
  BaseChannel.prototype.generateEditUI = function () { };
  BaseChannel.prototype.generateStatsUI = function () { };

  //
  // Functions for updating the UI as new data/configs come in.
  //
  BaseChannel.prototype.updateControlUI = function () { };
  BaseChannel.prototype.updateEditUI = function () { };
  BaseChannel.prototype.updateStatsUI = function () { };

  //
  // Functions for setting up the UI hooks and callbacks
  //
  BaseChannel.prototype.setupControlUI = function () { };

  BaseChannel.prototype.setupEditUI = function () {
    //populate our data
    $(`#f-${this.channelType}-name-${this.id}`).val(this.name);
    $(`#f-${this.channelType}-key-${this.id}`).val(this.key);
    $(`#f-${this.channelType}-enabled-${this.id}`).prop("checked", this.enabled);

    //enable/disable other stuff.
    $(`#f-${this.channelType}-name-${this.id}`).prop('disabled', !this.enabled);
    $(`#f-${this.channelType}-key-${this.id}`).prop('disabled', !this.enabled);

    //validate + save
    $(`#f-${this.channelType}-name-${this.id}`).change(this.onEditForm);
    $(`#f-${this.channelType}-enabled-${this.id}`).change(this.onEditForm);
    $(`#f-${this.channelType}-key-${this.id}`).change(this.onEditForm);
  };
  BaseChannel.prototype.setupStatsUI = function () {
    if (!this.enabled)
      $(`${this.channelType}Stats${this.id}`).hide();
  };

  YB.BaseChannel = BaseChannel;
  global.YB = YB; // expose to global

})(this);