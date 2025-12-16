(function (global) { //private scope

  // work in the global YB namespace.
  var YB = window.YB || {};

  function DigitalInputChannel() {
    YB.BaseChannel.call(this, "dio");
  }
  DigitalInputChannel.prototype = Object.create(YB.BaseChannel.prototype);
  DigitalInputChannel.prototype.constructor = DigitalInputChannel;

  DigitalInputChannel.prototype.parseConfig = function (cfg) {
    YB.BaseChannel.prototype.parseConfig.call(this, cfg);
  };

  YB.DigitalInputChannel = DigitalInputChannel;
  YB.ChannelRegistry.registerChannelType("dio", YB.DigitalInputChannel)

  window.YB = YB; // <-- this line makes it global

})(this); //private scope