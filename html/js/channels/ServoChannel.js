(function (global) { //private scope

  // work in the global YB namespace.
  var YB = global.YB || {};

  function ServoChannel() {
    YB.BaseChannel.call(this, "servo", "Servo");

    this.setAngle = this.setAngle.bind(this);
  }

  ServoChannel.prototype = Object.create(YB.BaseChannel.prototype);
  ServoChannel.prototype.constructor = ServoChannel;

  ServoChannel.currentSliderID = -1;

  ServoChannel.prototype.getConfigSchema = function () {
    // copy base schema to avoid mutating the base literal
    var base = YB.BaseChannel.prototype.getConfigSchema.call(this);

    var schema = Object.assign({}, base);
    return schema;
  };

  ServoChannel.prototype.generateControlUI = function () {
    return `
      <div id="servoControlCard${this.id}" class="col-12">
        <div class="p-3 bg-secondary border border-secondary rounded text-white">
          <!-- Header row -->
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h6 id="servoName${this.id}" class="mb-0 servoName">${this.name}</h6>
            <div class="input-group input-group-sm" style="width: 150px;">
              <span class="input-group-text">Angle</span>
              <input class="form-control text-end" id="servoAngle${this.id}">
              <span class="input-group-text">°</span>
            </div>
          </div>

          <!-- Slider -->
          <input type="range" class="form-range" min="0" max="180" step="1" id="servoSlider${this.id}" list="servoMarker${this.id}">
          <datalist id="servoMarker${this.id}">
            <option value="0"></option>
            <option value="30"></option>
            <option value="60"></option>
            <option value="90"></option>
            <option value="120"></option>
            <option value="150"></option>
            <option value="180"></option>
          </datalist>

          <!-- Optional tick labels under slider -->
          <div class="d-flex justify-content-between small text-white-50 mt-1">
            <span>0</span><span>30</span><span>60</span><span>90</span><span>120</span><span>150</span><span>180</span>
          </div>
        </div>
      </div>
    `;
  };

  ServoChannel.prototype.setupControlUI = function () {
    YB.BaseChannel.prototype.setupControlUI.call(this);

    const $slider = $('#servoSlider' + this.id);
    const $angle = $('#servoAngle' + this.id);

    //clear previous handlers
    $slider.off();
    $angle.off();

    // update while dragging
    $slider.on('input', this.setAngle);

    // on release: guarantee send
    $slider.on('change', (e) => {
      this._lastSend = Date.now() - 1000;
      this.setAngle(e);
    });

    // numeric input changes
    $angle.on('change', this.setAngle);

    // mark active while interacting
    $angle.on('focus', () => { ServoChannel.currentSliderID = this.id; });
    $slider.on('focus', () => { ServoChannel.currentSliderID = this.id; });

    //jquery doesnt allow passive
    // document.getElementById('servoSlider' + this.id).addEventListener('touchstart', () => {
    //   ServoChannel.currentSliderID = this.id;
    // }, { passive: true });

    // clear on stop
    const clearActive = () => { ServoChannel.currentSliderID = -1; };
    $angle.on('blur', clearActive);
    $slider.on('blur', clearActive);
    // document.getElementById('servoSlider' + this.id).addEventListener('touchend', clearActive, { passive: true });
  };

  ServoChannel.prototype.setAngle = function (e) {
    let angle = parseInt(e.target.value);
    if (isNaN(angle))
      angle = 0;
    angle = Math.min(180, Math.max(0, angle));

    $(`#servoAngle${this.id}`).val(angle);
    $(`#servoSlider${this.id}`).val(angle);

    const now = Date.now();
    if (!this._lastSend || now - this._lastSend > 250) {
      this._lastSend = now;
      YB.client.send({
        "cmd": "set_servo_channel",
        "id": this.id,
        "angle": angle
      });
    }
  }

  ServoChannel.prototype.generateEditUI = function () {

    let standardFields = this.generateStandardEditFields();

    return `
      <div id="servoEditCard${this.id}" class="col-xs-12 col-sm-6">
        <div class="p-3 border border-secondary rounded">
          <h5>Servo Channel #${this.id}</h5>
          ${standardFields}
        </div>
      </div>
    `;
  };

  ServoChannel.prototype.setupEditUI = function () {
    YB.BaseChannel.prototype.setupEditUI.call(this);
  };

  ServoChannel.prototype.updateControlUI = function () {
    if (ServoChannel.currentSliderID != this.id) {
      $('#servoSlider' + this.id).val(this.data.angle);
      $('#servoAngle' + this.id).val(this.data.angle);
    }

    $(`#servoControlCard${this.id}`).toggle(this.enabled);
  };

  YB.ServoChannel = ServoChannel;
  YB.ChannelRegistry.registerChannelType("servo", YB.ServoChannel)

  global.YB = YB; // <-- this line makes it global

})(this); //private scope