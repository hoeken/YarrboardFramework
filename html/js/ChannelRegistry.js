(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  // Channel registry + factory under YB
  YB.ChannelRegistry = {
    // Map of type keys to channel constructors
    // "pwm": YB.PWMChannel,
    // "relay": YB.RelayChannel,
    //  etc.
    channelConstructors: {},

    //this is where our actual channel objects will live
    // "pwm": [{ch1}, {ch2}, etc],
    // "relay": [{ch1}, {ch2}, etc]
    //  etc.
    channels: {},

    // Register or override a channel type
    registerChannelType: function (type, ctor) {
      this.channelConstructors[type] = ctor;
    },

    // Create a channel instance from a config object
    channelFromConfig: function (cfg, type) {
      if (!cfg)
        throw new Error('Invalid channel config.');

      var ctor = this.channelConstructors[type];
      if (!ctor)
        throw new Error("Unknown channel type: " + type);

      var instance = new ctor();
      let errors = instance.loadConfig(cfg);
      if (!errors)
        this.addChannel(instance);
      else
        console.error(errors);

      return instance;
    },

    addChannel(instance) {
      if (typeof instance.channelType !== "string" || !instance.channelType.length)
        throw new Error("Invalid channel type.");
      if (!instance)
        throw new Error("Channel instance is required.");

      // Ensure array exists
      if (!this.channels.hasOwnProperty(instance.channelType))
        this.channels[instance.channelType] = [];

      // Add it
      this.channels[instance.channelType].push(instance);

      return instance;
    },

    getChannelById(id, type) {
      if (typeof type !== "string" || !type.length)
        throw new Error("Type must be a non-empty string.");
      if (typeof id !== "number")
        throw new Error("ID must be a number.");

      var arr = this.channels[type];
      if (!arr || !arr.length)
        return null;

      for (var i = 0; i < arr.length; i++) {
        if (arr[i].id === id)
          return arr[i];
      }

      return null;
    },

    getChannelByKey(key, type) {
      if (typeof type !== "string" || !type.length)
        throw new Error("Type must be a non-empty string.");
      if (typeof key !== "string" || !key.length)
        throw new Error("Key must be a non-empty string.");

      var arr = this.channels[type];
      if (!arr || !arr.length)
        return null;

      for (var i = 0; i < arr.length; i++) {
        if (arr[i].key === key)
          return arr[i];
      }

      return null;
    },

    getChannelsByType(type) {
      if (typeof type !== "string" || !type.length)
        throw new Error("Type must be a non-empty string.");

      var arr = this.channels[type];
      if (!arr || !arr.length)
        return null;

      return arr;
    },

    loadAllChannels: function (cfg) {
      //loop through all of our register channel types and see if we got config data
      for (var ctype of Object.keys(this.channelConstructors)) {
        if (!cfg.hasOwnProperty(ctype))
          continue;

        //initialize our control container
        this.createChannelContainers(ctype);

        $(`#${ctype}ControlDiv`).hide();
        $(`#${ctype}Cards`).html("");
        $(`#${ctype}Config`).hide();
        $(`#${ctype}ConfigForm`).html("");
        $(`#${ctype}StatsDiv`).hide();
        $(`#${ctype}StatsTableBody`).html("");

        //handle each individual channels setup
        for (var channel_config of cfg[ctype]) {
          let ch = this.channelFromConfig(channel_config, ctype);

          let ui_card = ch.generateControlUI();
          $(`#${ctype}Cards`).append(ui_card);
          ch.setupControlUI();

          ch.generateStatsUI();
          ch.setupStatsUI();

          //we always want edit.
          let edit_card = ch.generateEditUI();
          $(`#${ctype}ConfigForm`).append(edit_card);
          ch.setupEditUI();
        }

        //show our containers now.
        $(`#${ctype}Config`).show();
        $(`#${ctype}StatsDiv`).show();
        $(`#${ctype}ControlDiv`).show();
      }
    },

    updateAllChannels: function (update) {
      for (var ctype of Object.keys(this.channels)) {
        if (!update.hasOwnProperty(ctype))
          continue;

        for (chdata of update[ctype]) {
          let ch = this.getChannelById(chdata.id, ctype);
          ch.loadData(chdata);

          if (ch.updateControlUI)
            ch.updateControlUI();
        }
      }
    },

    updateAllStats: function (update) {
      for (var ctype of Object.keys(this.channels)) {
        if (!update.hasOwnProperty(ctype))
          continue;

        var ctor = this.channelConstructors[ctype];
        let ch = new ctor();
        ch.resetStats();

        for (chdata of update[ctype]) {
          let ch = this.getChannelById(chdata.id, ctype);
          ch.updateStatsUI(chdata);
        }
      }
    },

    createChannelContainers: function (ctype) {
      var ctor = this.channelConstructors[ctype];
      if (!ctor)
        throw new Error("Unknown channel type: " + ctype);
      let ch = new ctor();

      if (!$(`#${ctype}ControlDiv`).length)
        $(`#controlPage`).append(ch.generateControlContainer());

      if (!$(`#${ctype}Config`).length)
        $(`#ConfigForm`).append(ch.generateEditContainer());

      if (!$(`#${ctype}StatsDiv`).length)
        $(`#statsContainer`).append(ch.generateStatsContainer());
    },

  };

  // expose to global
  global.YB = YB;

})(this);