(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  const YarrboardClient = window.YarrboardClient;

  YB.App = {
    config: {},

    messageHandlers: {},

    addMessageHandler: function (message, handler) {
      if (!YB.App.messageHandlers[message])
        YB.App.messageHandlers[message] = [];
      YB.App.messageHandlers[message].push(handler);
    },

    onMessage: function (msg) {
      if (msg.debug)
        YB.log(`[server] ${msg.debug}`);

      if (msg.msg) {
        const handlers = YB.App.messageHandlers[msg.msg];
        if (!handlers) {
          YB.log("No handler for: " + JSON.stringify(msg));
          return;
        }

        for (let handler of handlers) {
          handler(msg);
        }
      }
    },

    updateInterval: 500,
    updateIntervalId: null,

    username: null,
    password: null,
    role: "nobody",
    defaultRole: "nobody",

    currentPage: null,
    pageList: ["control", "config", "stats", "network", "settings", "system"],
    pageReady: {
      "control": false,
      "config": false,
      "stats": false,
      "network": false,
      "settings": false,
      "system": true,
      "login": true
    },

    pagePermissions: {
      "nobody": [
        "login"
      ],
      "admin": [
        "control",
        "config",
        "stats",
        "network",
        "settings",
        "system",
        "login",
        "logout"
      ],
      "guest": [
        "control",
        "stats",
        "login",
        "logout"
      ]
    },

    currentlyPickingBrightness: false,

    start: function () {
      YB.log.setupDebugTerminal();
      // YB.log("User Agent: " + navigator.userAgent);
      // YB.log("Window Width: " + window.innerWidth);
      // YB.log("Window Height: " + window.innerHeight);
      // YB.log("Window Location: " + window.location);
      // YB.log("Device Pixel Ratio: " + window.devicePixelRatio);
      // YB.log("Is canvas supported? " + YB.Util.isCanvasSupported());

      //main data connection
      YB.App.startWebsocket();

      //check our connection status.
      setInterval(YB.App.checkConnectionStatus, 100);

      //light/dark theme init.
      let theme = YB.App.getPreferredTheme();
      YB.App.setTheme(theme);
      $("#darkSwitch").change(YB.App.updateThemeSwitch);

      // preferred scheme watcher
      window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => {
        const storedTheme = YB.App.getStoredTheme();
        if (storedTheme !== 'light' && storedTheme !== 'dark') {
          YB.App.setTheme(YB.App.getPreferredTheme());
        }
      });

      //brightness slider callbacks
      $("#brightnessSlider").change(YB.App.setBrightness);
      $("#brightnessSlider").on("input", YB.App.setBrightness);

      //stop updating the UI when we are choosing
      $("#brightnessSlider").on('focus', function (e) {
        YB.App.currentlyPickingBrightness = true;
      });

      //stop updating the UI when we are choosing
      $("#brightnessSlider").on('touchstart', function (e) {
        YB.App.currentlyPickingBrightness = true;
      });

      //restart the UI updates when slider is closed
      $("#brightnessSlider").on("blur", function (e) {
        YB.App.currentlyPickingBrightness = false;
      });

      //restart the UI updates when slider is closed
      $("#brightnessSlider").on("touchend", function (e) {
        YB.App.currentlyPickingBrightness = false;
      });

      $("#saveGeneralSettings").on('click', YB.App.saveGeneralSettings);
      $("#saveAuthenticationSettings").on('click', YB.App.saveAuthenticationSettings);
      $("#saveWebServerSettings").on('click', YB.App.saveWebServerSettings);
      $("#saveMQTTSettings").on('click', YB.App.saveMQTTSettings);
      $("#saveNetworkSettings").on('click', YB.App.saveNetworkSettings);
      $("#saveMiscellaneousSettings").on('click', YB.App.saveMiscellaneousSettings);
    },

    startWebsocket: function () {
      //do we want ssl?
      let use_ssl = false;
      if (document.location.protocol == 'https:')
        use_ssl = true;

      //set it up
      YB.client = new YarrboardClient(window.location.host, "", "", false, use_ssl);

      //figure out what our login situation is
      YB.client.onopen = function () {
        YB.client.sayHello();
      };

      //app handles messages
      YB.client.onmessage = YB.App.onMessage;

      //use custom logger
      YB.client.log = YB.log;

      YB.client.start();
    },

    openPage: function (page) {
      //YB.log(`opening ${page}`);

      if (!YB.App.pagePermissions[YB.App.role].includes(page)) {
        YB.log(`${page} not allowed for ${YB.App.role}`);
        return;
      }

      YB.App.currentPage = page;

      //request our stats.
      if (page == "stats")
        YB.App.getStatsData();

      //request our control updates.
      if (page == "control")
        YB.App.startUpdateData();

      //we need updates for adc config page.
      if (page == "config" && YB.App.config && YB.App.config.hasOwnProperty("adc"))
        YB.App.startUpdateData();

      //hide all pages.
      $("div.pageContainer").hide();

      //special stuff
      if (page == "login") {
        //hide our nav bar
        $("#navbar").hide();

        //enter triggers login
        $(document).on('keypress', function (e) {
          if (e.which == 13)
            YB.App.doLogin();
        });
      }

      //sad to see you go.
      if (page == "logout") {
        Cookies.remove("username");
        Cookies.remove("password");

        YB.App.role = YB.App.defaultRole;
        YB.App.updateRoleUI();

        YB.client.logout();

        if (YB.App.role == "nobody")
          YB.App.openPage("login");
        else
          YB.App.openPage("control");
      }
      else {
        //update our nav
        $('#controlNav .nav-link').removeClass("active");
        $(`#${page}Nav a`).addClass("active");

        //is our new page ready?
        YB.App.onPageReady();
      }
    },

    isMFD: function () {
      if (YB.Util.getQueryVariable("mfd_name") !== null)
        return true;

      return false;
    },

    AlertBox: (message, type) => `
      <div>
        <div class="mt-3 alert alert-${type} alert-dismissible" role="alert">
          <div>${message}</div>
          <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
        </div>
      </div>`,

    loadConfigs: function () {
      if (YB.App.role == "nobody")
        return;

      YB.client.getConfig();

      if (YB.App.role == "admin") {
        YB.client.getNetworkConfig();
        YB.client.getAppConfig();
        YB.client.send({ "cmd": "get_full_config" });
      }
    },

    checkConnectionStatus: function () {
      if (YB.client) {
        let status = YB.client.status();

        //our connection status
        $(".connection_status").hide();

        if (status == "CONNECTING")
          $("#connection_startup").show();
        else if (status == "CONNECTED")
          $("#connection_good").show();
        else if (status == "RETRYING")
          $("#connection_retrying").show();
        else if (status == "FAILED")
          $("#connection_failed").show();
      }
    },

    showAlert: function (message, type = 'danger') {
      //we only need one alert at a time.
      $('#liveAlertPlaceholder').html(YB.App.AlertBox(message, type))

      //make sure we can see it.
      $('html').animate({
        scrollTop: 0
      },
        750 //speed
      );
    },

    showAdminAlert: function (message, type = 'danger') {
      //we only need one alert at a time.
      $('#adminAlertPlaceholder').html(YB.App.AlertBox(message, type))

      //make sure we can see it.
      $('html').animate({
        scrollTop: 0
      },
        750 //speed
      );
    },

    addConfigurationDragDropHandler: function () {
      const ta = document.getElementById('configurationTextarea');
      const invalid = document.getElementById('invalidConfigurationJSON');
      const openBtn = document.getElementById('configurationOpen');
      const fileInput = document.getElementById('configurationFileInput');
      const MAX_BYTES = 1024 * 1024 * 2; // 2 MB cap

      if (!ta) return;

      function showInvalid(msg) {
        if (invalid) {
          invalid.style.display = '';
          invalid.textContent = msg || 'Invalid JSON found.';
        }
      }
      function hideInvalid() {
        if (invalid) invalid.style.display = 'none';
      }

      async function handleFile(file) {
        if (!file) return;

        if (file.size > MAX_BYTES) {
          showInvalid(`File too large (${(file.size / 1024 / 1024).toFixed(1)} MB). Max ${(MAX_BYTES / 1024 / 1024)} MB.`);
          return;
        }

        const isProbablyText = file.type.startsWith('text/') || /\.json$/i.test(file.name);
        if (!isProbablyText) {
          showInvalid('Please choose a .json or text file.');
          return;
        }

        try {
          const raw = await file.text();
          try {
            const obj = JSON.parse(raw);
            ta.value = JSON.stringify(obj, null, 2);
            hideInvalid();
          } catch {
            ta.value = raw;
            showInvalid('Loaded file, but JSON is invalid.');
          }
          ta.dispatchEvent(new Event('input', { bubbles: true }));
        } catch (err) {
          showInvalid('Failed to read file.');
          console.error(err);
        }
      }

      // Drag & drop visuals
      ['dragenter', 'dragover'].forEach(evt =>
        ta.addEventListener(evt, (e) => {
          e.preventDefault();
          e.stopPropagation();
          ta.classList.add('is-drop-target');
        })
      );
      ['dragleave', 'dragend'].forEach(evt =>
        ta.addEventListener(evt, (e) => {
          e.preventDefault();
          e.stopPropagation();
          ta.classList.remove('is-drop-target');
        })
      );

      // Drop handler
      ta.addEventListener('drop', async (e) => {
        e.preventDefault();
        e.stopPropagation();
        ta.classList.remove('is-drop-target');

        const files = e.dataTransfer && e.dataTransfer.files;
        if (files && files.length > 0) {
          handleFile(files[0]);
        }
      });

      // Open button triggers hidden input
      if (openBtn && fileInput) {
        openBtn.addEventListener('click', () => fileInput.click());
        fileInput.addEventListener('change', (e) => {
          const file = e.target.files && e.target.files[0];
          handleFile(file);
          // allow re-selecting the same file later
          e.target.value = '';
        });
      }
    },

    onPageReady: function () {
      //is our page ready yet?
      if (YB.App.pageReady[YB.App.currentPage]) {
        $("#loading").hide();
        $(`#${YB.App.currentPage}Page`).show();
      }
      else {
        $("#loading").show();
        setTimeout(YB.App.onPageReady, 100);
      }
    },

    getStatsData: function () {
      if (YB.client.isOpen() && (YB.App.role == 'guest' || YB.App.role == 'admin')) {
        //YB.log("get_stats");

        YB.client.getStats();

        //keep loading it while we are here.
        if (YB.App.currentPage == "stats")
          setTimeout(YB.App.getStatsData, YB.App.updateInterval);
      }
    },

    startUpdateData: function () {
      if (!YB.App.updateIntervalId) {
        //YB.log("starting updates");
        YB.App.updateIntervalId = setInterval(YB.App.getUpdateData, YB.App.updateInterval);
      } else {
        //YB.log("updates already running");
      }
    },

    getUpdateData: function () {
      if (YB.client.isOpen() && (YB.App.role == 'guest' || YB.App.role == 'admin')) {
        //YB.log("get_update");

        YB.client.getUpdate();

        //keep loading it while we are here.
        if (YB.App.currentPage == "control" || (YB.App.currentPage == "config" && YB.App.config && YB.App.config.hasOwnProperty("adc")))
          return;
        else {
          //YB.log("stopping updates");
          clearInterval(YB.App.updateIntervalId);
          YB.App.updateIntervalId = 0;
        }
      }
    },

    setBrightness: function (e) {
      let ele = e.target;
      let value = ele.value;

      //must be realistic.
      if (value >= 0 && value <= 100) {
        //we want a duty value from 0 to 1
        value = value / 100;

        //update our duty cycle
        YB.client.setBrightness(value, false);
      }
    },

    updateThemeSwitch: function (e) {
      YB.App.setTheme(e.target.checked ? "dark" : "light");
    },

    doLogin: function (e) {
      YB.App.username = $('#username').val();
      YB.App.password = $('#password').val();
      YB.client.login(YB.App.username, YB.App.password);
    },

    getGeneralSettingsSchema: function () {
      return {
        board_name: {
          presence: { allowEmpty: false },
          length: { maximum: 31 },
        },
        startup_melody: {
          presence: { allowEmpty: false },
          inclusion: {
            within: YB.App.config.melodies,
            message: "^Invalid melody selection"
          }
        }
      };
    },

    getAuthenticationSettingsSchema: function () {
      return {
        admin_user: {
          presence: { allowEmpty: false },
          length: { maximum: 31 },
          format: {
            pattern: "^[a-zA-Z0-9]+$",
            message: "can only contain letters and numbers"
          }
        },
        admin_pass: {
          presence: { allowEmpty: false },
          length: { maximum: 63 }
        },
        guest_user: {
          presence: { allowEmpty: false },
          length: { maximum: 31 },
          format: {
            pattern: "^[a-zA-Z0-9]+$",
            message: "can only contain letters and numbers"
          }
        },
        guest_pass: {
          presence: { allowEmpty: false },
          length: { maximum: 63 }
        },
        default_role: {
          presence: true,
          inclusion: {
            within: ["nobody", "guest", "admin"],
            message: "^Default role must be one of: nobody, guest, or admin"
          }
        }
      };
    },

    saveGeneralSettings: function () {
      //pull our form data
      const settings = {
        board_name: $("#board_name").val().trim(),
        startup_melody: $("#startup_melody").val(),
      };

      //validate it.
      const errors = validate(settings, YB.App.getGeneralSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      //bail on fail.
      if (errors) {
        YB.Util.flashClass($("#generalSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveGeneralSettings"), "btn-danger");
        return;
      }

      //remember it and update our UI
      YB.App.updateBoardName(settings.board_name);

      //flash whole form green.
      YB.Util.flashClass($("#generalSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveGeneralSettings"), "btn-success");

      //okay, send it off.
      YB.client.send({
        "cmd": "set_general_config",
        "board_name": settings.board_name,
        "startup_melody": settings.startup_melody
      });
    },

    saveAuthenticationSettings: function () {
      //pull our form data
      const settings = {
        admin_user: $("#admin_user").val().trim(),
        admin_pass: $("#admin_pass").val().trim(),
        guest_user: $("#guest_user").val().trim(),
        guest_pass: $("#guest_pass").val().trim(),
        default_role: $("#default_role option:selected").val()
      };

      //validate it.
      const errors = validate(settings, YB.App.getAuthenticationSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      //bail on fail.
      if (errors) {
        YB.Util.flashClass($("#authenticationSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveAuthenticationSettings"), "btn-danger");
        return;
      }

      //remember it and update our UI
      YB.App.defaultRole = settings.default_role;
      YB.App.updateRoleUI();

      //helper function to keep admin logged in.
      if (YB.App.defaultRole != "admin") {
        Cookies.set('username', settings.admin_user, { expires: 365 });
        Cookies.set('password', settings.admin_pass, { expires: 365 });
      }
      else {
        Cookies.remove("username");
        Cookies.remove("password");
      }

      //flash whole form green.
      YB.Util.flashClass($("#authenticationSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveAuthenticationSettings"), "btn-success");

      //okay, send it off.
      YB.client.send({
        "cmd": "set_authentication_config",
        "admin_user": settings.admin_user,
        "admin_pass": settings.admin_pass,
        "guest_user": settings.guest_user,
        "guest_pass": settings.guest_pass,
        "default_role": settings.default_role
      });
    },

    getWebServerSettingsSchema: function () {
      return {
        // checkboxes
        app_enable_mfd: {},
        app_enable_api: {},
        app_enable_ssl: {},

        // required only if SSL is enabled
        server_cert: function (value, attrs) {
          if (attrs.app_enable_ssl) {
            return { presence: { allowEmpty: false, message: "^is required when SSL is enabled" } };
          }
          return {};
        },
        server_key: function (value, attrs) {
          if (attrs.app_enable_ssl) {
            return { presence: { allowEmpty: false, message: "^is required when SSL is enabled" } };
          }
          return {};
        }
      };
    },

    saveWebServerSettings: function () {
      // pull our form data
      const settings = {
        app_enable_mfd: $("#app_enable_mfd").prop("checked"),
        app_enable_api: $("#app_enable_api").prop("checked"),
        app_enable_ssl: $("#app_enable_ssl").prop("checked"),
        server_cert: $("#server_cert").val().trim(),
        server_key: $("#server_key").val().trim()
      };

      // validate it
      const errors = validate(settings, YB.App.getWebServerSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      // bail on fail
      if (errors) {
        YB.Util.flashClass($("#webServerSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveWebServerSettings"), "btn-danger");
        return;
      }

      // flash whole form green
      YB.Util.flashClass($("#webServerSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveWebServerSettings"), "btn-success");

      // okay, send it off.
      YB.client.send({
        cmd: "set_webserver_config",
        app_enable_mfd: settings.app_enable_mfd,
        app_enable_api: settings.app_enable_api,
        app_enable_ssl: settings.app_enable_ssl,
        server_cert: settings.server_cert,
        server_key: settings.server_key
      });
    },

    getMQTTSettingsSchema: function () {
      return {
        //checkboxes
        app_enable_mqtt: {},
        app_enable_ha_integration: {},
        app_use_hostname_as_mqtt_uuid: {},

        // required if MQTT is enabled; must be protocol://host[:port]
        mqtt_server: function (value, attrs) {
          if (!attrs.app_enable_mqtt) return {};
          return {
            presence: { allowEmpty: false, message: "^is required when MQTT is enabled" },
            format: {
              // scheme://hostname[:port] — allows IPv4/hostname/IPv6([::]) and optional port
              pattern: /^[a-zA-Z][a-zA-Z0-9+.\-]*:\/\/(?:\[[A-Fa-f0-9:]+\]|[A-Za-z0-9.-]+)(?::\d{1,5})?$/,
              message: "^must be a URL like mqtt://host[:port]"
            }
          };
        },

        // optional creds
        mqtt_user: {
          length: { maximum: 31 }
        },
        mqtt_pass: {
          length: { maximum: 63 }
        },

        // optional cert (no constraints)
        mqtt_cert: {}
      };
    },

    saveMQTTSettings: function () {
      // pull our form data
      const settings = {
        app_enable_mqtt: $("#app_enable_mqtt").prop("checked"),
        app_enable_ha_integration: $("#app_enable_ha_integration").prop("checked"),
        app_use_hostname_as_mqtt_uuid: $("#app_use_hostname_as_mqtt_uuid").prop("checked"),
        mqtt_server: $("#mqtt_server").val().trim(),
        mqtt_user: $("#mqtt_user").val().trim(),
        mqtt_pass: $("#mqtt_pass").val().trim(),
        mqtt_cert: $("#mqtt_cert").val().trim()
      };

      // validate it
      const errors = validate(settings, YB.App.getMQTTSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      // bail on fail
      if (errors) {
        YB.Util.flashClass($("#mqttSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveMQTTSettings"), "btn-danger");
        return;
      }

      // flash whole form green
      YB.Util.flashClass($("#mqttSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveMQTTSettings"), "btn-success");

      // okay, send it off
      YB.client.send({
        cmd: "set_mqtt_config",
        app_enable_mqtt: settings.app_enable_mqtt,
        app_enable_ha_integration: settings.app_enable_ha_integration,
        app_use_hostname_as_mqtt_uuid: settings.app_use_hostname_as_mqtt_uuid,
        mqtt_server: settings.mqtt_server,
        mqtt_user: settings.mqtt_user,
        mqtt_pass: settings.mqtt_pass,
        mqtt_cert: settings.mqtt_cert
      });
    },

    getNetworkSettingsSchema: function () {
      return {
        // either 'ap' or 'client'
        wifi_mode: {
          presence: { allowEmpty: false },
          inclusion: {
            within: ["ap", "client"],
            message: "^must be either 'ap' or 'client'"
          }
        },

        // required, valid Wi-Fi SSID (1–32 chars)
        wifi_ssid: {
          presence: { allowEmpty: false },
          length: { minimum: 1, maximum: 32 },
          format: {
            // disallow leading/trailing whitespace and newlines
            pattern: /^(?!\s)(?:[\s\S]*\S)$/,
            message: "^can't be only whitespace"
          }
        },

        // optional; if provided, validate WPA2 PSK length 8–63 chars
        wifi_pass: function (value/*, attrs */) {
          if (value == null || value === "") return {};
          return {
            length: {
              minimum: 8,
              maximum: 63,
              message: "^must be 8–63 characters"
            }
          };
        },

        // required; valid single-label mDNS hostname (before .local)
        local_hostname: {
          presence: { allowEmpty: false },
          length: { minimum: 1, maximum: 63 },
          format: {
            // RFC 1035 label: alnum + hyphen allowed, no leading/trailing hyphen
            pattern: /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/,
            message: "^must be a valid hostname label (e.g., 'brineomatic')"
          }
        }
      };
    },

    saveNetworkSettings: function () {
      // pull our form data
      const settings = {
        wifi_mode: $("#wifi_mode").val(),
        wifi_ssid: $("#wifi_ssid").val().trim(),
        wifi_pass: $("#wifi_pass").val().trim(),
        local_hostname: $("#local_hostname").val().trim()
      };

      // validate it
      const errors = validate(settings, YB.App.getNetworkSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      // bail on fail
      if (errors) {
        YB.Util.flashClass($("#networkSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveNetworkSettings"), "btn-danger");
        return;
      }

      // warn user before applying network changes
      YB.App.showAlert(
        "Yarrboard may be unresponsive while changing WiFi settings. Make sure you connect to the right network after updating.",
        "primary"
      );

      // flash success
      YB.Util.flashClass($("#networkSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveNetworkSettings"), "btn-success");

      // send it off
      YB.client.send({
        cmd: "set_network_config",
        wifi_mode: settings.wifi_mode,
        wifi_ssid: settings.wifi_ssid,
        wifi_pass: settings.wifi_pass,
        local_hostname: settings.local_hostname
      });

      // reload page after delay
      setTimeout(function () {
        location.reload();
      }, 2500);
    },

    getMiscSettingsSchema: function () {
      return {
        app_enable_serial: {},
        app_enable_ota: {}
      };
    },

    saveMiscellaneousSettings: function () {
      // pull our form data
      const settings = {
        app_enable_serial: $("#app_enable_serial").prop("checked"),
        app_enable_ota: $("#app_enable_ota").prop("checked")
      };

      // validate it
      const errors = validate(settings, YB.App.getMiscSettingsSchema());
      YB.Util.showFormValidationResults(settings, errors);

      // bail on fail
      if (errors) {
        YB.Util.flashClass($("#miscSettingsForm"), "border-danger");
        YB.Util.flashClass($("#saveMiscSettings"), "btn-danger");
        return;
      }

      // flash success
      YB.Util.flashClass($("#miscSettingsForm"), "border-success");
      YB.Util.flashClass($("#saveMiscSettings"), "btn-success");

      // send it off
      YB.client.send({
        cmd: "set_misc_config",
        app_enable_serial: settings.app_enable_serial,
        app_enable_ota: settings.app_enable_ota
      });
    },

    restartBoard: function () {
      if (confirm("Are you sure you want to restart your Yarrboard?")) {
        YB.client.restart();

        YB.App.showAlert("Yarrboard is now restarting, please be patient.", "primary");

        setTimeout(function () {
          location.reload();
        }, 5000);
      }
    },

    resetToFactory: function () {
      if (confirm("WARNING! Are you sure you want to reset your Yarrboard to factory defaults?  This cannot be undone.")) {
        YB.client.factoryReset();

        YB.App.showAlert("Yarrboard is now resetting to factory defaults, please be patient.", "primary");
      }
    },

    checkForUpdates: function () {
      //did we get a config yet?
      if (YB.App.config) {
        //dont look up firmware if we're in development mode.
        if (YB.App.config.is_development) {
          $("#firmware_checking").html("Development Mode, automatic firmware checking disabled.")
          return;
        }

        $.ajax({
          url: "https://raw.githubusercontent.com/hoeken/yarrboard-firmware/main/firmware.json",
          cache: false,
          dataType: "json",
          success: function (jdata) {
            //did we get anything?
            let data;
            for (firmware of jdata)
              if (firmware.type == YB.App.config.hardware_version)
                data = firmware;

            if (!data) {
              YB.App.showAlert(`Could not find a firmware for this hardware.`, "danger");
              return;
            }

            $("#firmware_checking").hide();

            //do we have a new version?
            if (YB.Util.compareVersions(data.version, YB.App.config.firmware_version)) {
              if (data.changelog) {
                $("#firmware_changelog").append(marked.parse(data.changelog));
                $("#firmware_changelog").show();
              }

              $("#new_firmware_version").html(data.version);
              $("#firmware_bin").attr("href", `${data.url}`);
              $("#firmware_update_available").show();

              YB.App.showAlert(`There is a <a onclick="YB.App.openPage('system')" href="/#system">firmware update</a> available (${data.version}).`, "primary");
            }
            else
              $("#firmware_up_to_date").show();
          }
        });
      }
    },

    updateFirmware: function () {
      $("#btn_update_firmware").prop("disabled", true);
      $("#progress_wrapper").show();

      //okay, send it off.
      YB.client.startOTA();
    },

    toggleRolePasswords: function (role) {
      if (role == "admin") {
        $(".admin_credentials").hide();
        $(".guest_credentials").hide();
      }
      else if (role == "guest") {
        $(".admin_credentials").show();
        $(".guest_credentials").hide();
      }
      else {
        $(".admin_credentials").show();
        $(".guest_credentials").show();
      }
    },

    updateRoleUI: function () {
      //what nav tabs should we be able to see?
      if (YB.App.role == "admin") {
        $("#navbar").show();
        $(".nav-permission").show();
      }
      else if (YB.App.role == "guest") {
        $("#navbar").show();
        $(".nav-permission").hide();
        YB.App.pagePermissions[YB.App.role].forEach((page) => {
          $(`#${page}Nav`).show();
        });
      }
      else {
        $("#navbar").hide();
        $(".nav-permission").hide();
      }

      //show login or not?
      $('#loginNav').hide();
      if (YB.App.defaultRole == 'nobody' && YB.App.role == 'nobody')
        $('#loginNav').show();
      if (YB.App.defaultRole == 'guest' && YB.App.role == 'guest')
        $('#loginNav').show();

      //show logout or not?
      $('#logoutNav').hide();
      if (YB.App.defaultRole == 'nobody' && YB.App.role != 'nobody')
        $('#logoutNav').show();
      if (YB.App.defaultRole == 'guest' && YB.App.role == 'admin')
        $('#logoutNav').show();
    },

    openDefaultPage: function () {
      if (YB.App.role != 'nobody') {
        //check to see if we want a certain page
        if (window.location.hash) {
          let page = window.location.hash.substring(1);
          if (page != "login" && YB.App.pageList.includes(page))
            YB.App.openPage(page);
          else
            YB.App.openPage("control");
        }
        else
          YB.App.openPage("control");
      }
      else
        YB.App.openPage('login');
    },

    getStoredTheme: function () { localStorage.getItem('theme'); },
    setStoredTheme: function () { localStorage.setItem('theme', theme); },

    getPreferredTheme: function () {

      // if browser supports prefers-color-scheme
      if (window.matchMedia('(prefers-color-scheme: dark)').media !== 'not all') {
        if (window.matchMedia('(prefers-color-scheme: dark)').matches)
          return "dark";
        if (window.matchMedia('(prefers-color-scheme: light)').matches)
          return "light";
      }

      //do we have stored one?
      const storedTheme = YB.App.getStoredTheme();
      // YB.log(`stored: ${storedTheme}`);
      if (storedTheme)
        return storedTheme;

      //did we get one passed in? b&g, etc pass in like this.
      let mode = YB.Util.getQueryVariable("mode");
      // YB.log(`mode: ${mode}`);
      if (mode !== null) {
        if (mode == "night")
          return "dark";
        else
          return "light";
      }

      //default to light mode
      return "light";
    },

    setTheme: function (theme) {
      if (theme === 'auto' && window.matchMedia('(prefers-color-scheme: dark)').matches)
        document.documentElement.setAttribute('data-bs-theme', 'dark');
      else
        document.documentElement.setAttribute('data-bs-theme', theme);

      let currentTheme = document.documentElement.getAttribute('data-bs-theme');
      if (currentTheme == "light")
        $("#darkSwitch").prop('checked', false);
      else
        $("#darkSwitch").prop('checked', true);
    },

    updateBoardName: function (name) {
      $('#boardName').html(name);
      $('#loginTitle').html(name);
      document.title = name;
      $('#board_name').val(name);
    },

    handleHelloMessage: function (msg) {
      YB.App.role = msg.role;
      YB.App.defaultRole = msg.default_role;

      //custom board names.
      YB.App.updateBoardName(msg.name);

      //light/dark mode
      //let the mfd override with ?mode=night, etc.
      if (!YB.Util.getQueryVariable("mode")) {
        if (msg.theme)
          YB.App.setTheme(msg.theme);
      }

      //auto login?
      if (Cookies.get("username") && Cookies.get("password")) {
        YB.client.login(Cookies.get("username"), Cookies.get("password"));
      } else {
        YB.App.updateRoleUI();
        YB.App.loadConfigs();
        YB.App.openDefaultPage();
      }
    },

    handleStatusMessage: function (msg) {
      if (msg.status == "error")
        YB.App.showAlert(msg.message, "danger");
      else if (msg.status == "success")
        YB.App.showAlert(msg.message, "success");
      else
        YB.App.showAlert(msg.message, "primary");
    },

    handleConfigMessage: function (msg) {
      // YB.log("config");
      // YB.log(msg);

      YB.App.config = msg;

      //we can check for new firmware now.
      YB.App.checkForUpdates();

      //is it our first boot?
      if (msg.first_boot && YB.App.currentPage != "network")
        YB.App.showAlert(`Welcome to Yarrboard, head over to <a href="#network" onclick="YB.App.openPage('network')">Network</a> to setup your WiFi.`, "primary");

      //did we get a crash?
      if (msg.has_coredump)
        YB.App.showAdminAlert(/*html*/ `
          <p>Oops, looks like Yarrboard crashed.</p>
          <p>Please download the <a href="/coredump.bin" target="_blank">coredump</a> and report it to our <a href="https://github.com/hoeken/yarrboard/issues">Github Issue Tracker</a> along with the following information:</p>
          <ul><li>Firmware: ${msg.firmware_version}</li><li>Hardware: ${msg.hardware_version}</li></ul>
        `, "danger");

      //send our boot log
      if (msg.boot_log) {
        var lines = msg.boot_log.split("\n");
        for (let line of lines) {
          line = line.trim();
          if (line)
            YB.log(`[server.boot] ${line}`);
        }
      }

      //let the people choose their own names!
      YB.App.updateBoardName(msg.name);

      //update our footer automatically.
      $('#projectName').html("Yarrboard v" + msg.firmware_version);

      //all our versions
      $("#firmware_version").html(`v${msg.firmware_version}`);

      //deal with our hash
      let clean_hash = msg.git_hash;
      let is_dirty = false;
      if (clean_hash.endsWith("-dirty")) {
        is_dirty = true;
        clean_hash = clean_hash.slice(0, -6); // remove the "-dirty" suffix
      }
      let short_hash = clean_hash.substring(0, 7); // for display
      if (is_dirty)
        short_hash += "-dirty";
      $("#git_hash").html(`<a href="https://github.com/hoeken/yarrboard-firmware/commit/${clean_hash}">${short_hash}</a>`);

      //various other component versions
      $("#build_time").html(msg.build_time);
      $("#hardware_version").html(msg.hardware_version);
      $("#esp_idf_version").html(`${msg.esp_idf_version}`);
      $("#arduino_version").html(`v${msg.arduino_version}`);
      $("#psychic_http_version").html(`v${msg.psychic_http_version}`);
      $("#yarrboard_client_version").html(`v${YarrboardClient.version}`);

      //show some info about restarts
      if (msg.last_restart_reason)
        $("#last_reboot_reason").html(msg.last_restart_reason);
      else
        $("#last_reboot_reason").parent().hide();

      YB.ChannelRegistry.loadAllChannels(msg);

      //populate our adc control table
      $('#adcControlDiv').hide();
      if (msg.adc) {
        $('#adcTableBody').html("");
        for (ch of msg.adc) {
          if (ch.enabled)
            $('#adcTableBody').append(ADCControlRow(ch.id, ch.name, ch.type));
        }

        $('#adcControlDiv').show();

        //start our update poller.
        if (YB.App.currentPage == 'config')
          YB.App.startUpdateData();
      }

      //only do it as needed
      if (!YB.App.pageReady.config || YB.App.currentPage != "config") {

        //edit controls for each adc
        $('#adcConfig').hide();
        if (msg.adc) {
          $('#adcConfigForm').html("");
          for (ch of msg.adc) {
            $('#adcConfigForm').append(ADCEditCard(ch));
            $(`#fADCEnabled${ch.id}`).prop("checked", ch.enabled);
            $(`#fADCType${ch.id}`).val(ch.type);
            $(`#fADCDisplayDecimals${ch.id}`).val(ch.displayDecimals);
            $(`#fADCUseCalibrationTable${ch.id}`).prop("checked", ch.useCalibrationTable);
            if (ch.useCalibrationTable)
              $(`#fADCCalibrationTableUI${ch.id}`).show();
            else
              $(`#fADCCalibrationTableUI${ch.id}`).hide();

            //enable/disable other stuff.
            $(`#fADCName${ch.id}`).prop('disabled', !ch.enabled);
            $(`#fADCType${ch.id}`).prop('disabled', !ch.enabled);
            $(`#fADCDisplayDecimals${ch.id}`).prop('disabled', !ch.enabled);
            $(`#fADCUseCalibrationTable${ch.id}`).prop('disabled', !ch.enabled);

            //validate + save
            $(`#fADCEnabled${ch.id}`).change(validate_adc_enabled);
            $(`#fADCName${ch.id}`).change(validate_adc_name);
            $(`#fADCType${ch.id}`).change(validate_adc_type);
            $(`#fADCDisplayDecimals${ch.id}`).change(validate_adc_display_decimals);
            $(`#fADCUseCalibrationTable${ch.id}`).change(validate_adc_use_calibration_table);
            $(`#fADCCalibratedUnits${ch.id}`).change(validate_adc_calibrated_units);

            //calibration table stuff.
            $(`#fADCAverageOutputCopy${ch.id}`).click(adc_calibration_average_copy);
            $(`#fADCAverageOutputReset${ch.id}`).click(adc_calibration_average_reset);
            $(`#fADCCalibrationTableAdd${ch.id}`).click(validate_adc_add_calibration);
            if (YB.App.config.adc[ch.id - 1].hasOwnProperty("calibrationTable")) {
              YB.App.config.adc[ch.id - 1].calibrationTable.map(([output, calibrated], index) => {
                $(`#fADCCalibrationTableRemove${ch.id}_${index}`).click(validate_adc_remove_calibration);
              });
            }

            //for our live averages on config page.
            adc_running_averages[ch.id] = [];
          }

          $('#adcConfig').show();
        }
      }

      //did we get brightness?
      if (msg.brightness && !YB.App.currentlyPickingBrightness)
        $('#brightnessSlider').val(Math.round(msg.brightness * 100));

      if (YB.App.isMFD()) {
        $(".mfdShow").show()
        $(".mfdHide").hide()
      }
      else {
        $(".mfdShow").hide()
        $(".mfdHide").show()
      }

      YB.Util.populateMelodySelector($("#startup_melody"));

      //ready!
      YB.App.pageReady.config = true;

      if (!YB.App.currentPage)
        YB.App.openPage('control');
    },

    handleUpdateMessage: function (msg) {
      // YB.log("update");
      // YB.log(msg);

      //we need a config loaded.
      if (!YB.App.config)
        return;

      YB.ChannelRegistry.updateAllChannels(msg);

      //update our clock.
      // let mytime = Date.parse(msg.time);
      // if (mytime)
      // {
      //   let mydate = new Date(mytime);
      //   $('#time').html(mydate.toLocaleString());
      //   $('#time').show();
      // }
      // else
      //   $('#time').hide();

      // if (msg.uptime)
      //   $("#uptime").html(YB.Util.secondsToDhms(Math.round(msg.uptime/1000000)));

      //or maybe voltage
      // if (msg.bus_voltage)
      // {
      //   $('#bus_voltage_main').html("Bus Voltage: " + msg.bus_voltage.toFixed(2) + "V");
      //   $('#bus_voltage_main').show();
      // }
      // else
      //   $('#bus_voltage_main').hide();

      //our adc info
      if (msg.adc) {
        for (ch of msg.adc) {
          if (YB.App.config.adc[ch.id - 1].enabled) {

            //load our defaults
            let units = YB.App.config.adc[ch.id - 1].units;
            let value = parseFloat(ch.value);
            let calibrated_value = value;

            // let adc_raw = Math.round(ch.adc_raw);
            // let adc_voltage = ch.adc_voltage.toFixed(2);
            // $("#adcRaw" + ch.id).html(adc_raw);
            // $("#adcVoltage" + ch.id).html(adc_voltage + "V")
            //let percentage = ch.percentage.toFixed(1);
            // $(`#adcBar${ch.id} div`).css("width", percentage + "%");
            // $(`#adcBar${ch.id}`).attr("aria-valuenow", percentage);

            //are we using a calibration table?
            if (YB.App.config.adc[ch.id - 1].useCalibrationTable) {
              units = YB.App.config.adc[ch.id - 1].calibratedUnits;
              calibrated_value = parseFloat(ch.calibrated_value);
            }

            //save it to our running average.
            if (YB.App.currentPage == "config") {
              //manage our sliding window
              adc_running_averages[ch.id].push(value);
              if (adc_running_averages[ch.id].length > 1000)
                adc_running_averages[ch.id].shift();

              //update our average
              const average = adc_running_averages[ch.id].reduce((accumulator, currentValue) => accumulator + currentValue, 0) / adc_running_averages[ch.id].length;
              $(`#fADCAverageOutput${ch.id}`).val(average.toFixed(4));
              $(`#fADCAverageOutputCount${ch.id}`).html(`(${adc_running_averages[ch.id].length} points)`);
            } else
              adc_running_averages[ch.id] = [];

            //how should we format our value?
            if (YB.App.config.adc[ch.id - 1].displayDecimals >= 0) {
              calibrated_value = YB.Util.formatNumber(calibrated_value, YB.App.config.adc[ch.id - 1].displayDecimals);
            } else {
              switch (YB.App.config.adc[ch.id - 1].type) {
                case "thermistor":
                  calibrated_value = YB.Util.formatNumber(calibrated_value, 1);
                  break;
                case "raw":
                case "4-20ma":
                case "high_volt_divider":
                case "low_volt_divider":
                  calibrated_value = YB.Util.formatNumber(calibrated_value, 2);
                  break;
                case "ten_k_pullup":
                  calibrated_value = YB.Util.formatNumber(calibrated_value, 0);
                  break;
                default:
                  calibrated_value = YB.Util.formatNumber(calibrated_value, 3);
              }
            }

            //how should we display our value?
            switch (YB.App.config.adc[ch.id - 1].type) {
              case "digital_switch":
                if (value)
                  $("#adcValue" + ch.id).html(`HIGH`);
                else
                  $("#adcValue" + ch.id).html(`LOW`);
                break;
              case "4-20ma":
                if (value == 0)
                  $("#adcValue" + ch.id).html(`<span class="text-danger">ERROR: No Signal</span>`);
                else if (value < 4.0)
                  $("#adcValue" + ch.id).html(`<span class="text-danger">ERROR: ???</span>`);
                else
                  $("#adcValue" + ch.id).html(`${calibrated_value}${units}`);
                break;
              case "ten_k_pullup":
                if (value == -2)
                  $("#adcValue" + ch.id).html(`<span class="text-danger">Error: No Connection</span>`);
                else if (value == -1)
                  $("#adcValue" + ch.id).html(`<span class="text-danger">Error: Negative Voltage</span>`);
                else
                  $("#adcValue" + ch.id).html(`${calibrated_value}${units}`);
                break;

              default:
                $("#adcValue" + ch.id).html(`${calibrated_value}${units}`);
            }
          }
        }
      }

      if (YB.App.isMFD()) {
        $(".mfdShow").show()
        $(".mfdHide").hide()
      }
      else {
        $(".mfdShow").hide()
        $(".mfdHide").show()
      }

      YB.App.pageReady.control = true;
    },

    handleStatsMessage: function (msg) {
      //YB.log("stats");

      //we need this
      if (!YB.App.config)
        return;

      YB.ChannelRegistry.updateAllStats(msg);

      $("#uptime").html(YB.Util.secondsToDhms(Math.round(msg.uptime / 1000000)));
      if (msg.fps)
        $("#fps").html(msg.fps.toLocaleString("en-US") + " hz");

      //message info
      $("#received_message_mps").html(msg.received_message_mps.toLocaleString("en-US") + " mps");
      $("#received_message_total").html(msg.received_message_total.toLocaleString("en-US"));
      $("#sent_message_mps").html(msg.sent_message_mps.toLocaleString("en-US") + " mps");
      $("#sent_message_total").html(msg.sent_message_total.toLocaleString("en-US"));

      //client info
      $("#total_client_count").html(msg.http_client_count + msg.websocket_client_count);
      $("#http_client_count").html(msg.http_client_count);
      $("#websocket_client_count").html(msg.websocket_client_count);

      //raw data
      $("#heap_size").html(YB.Util.formatBytes(msg.heap_size, 0));
      $("#free_heap").html(YB.Util.formatBytes(msg.free_heap, 0));
      $("#min_free_heap").html(YB.Util.formatBytes(msg.min_free_heap, 0));
      $("#max_alloc_heap").html(YB.Util.formatBytes(msg.max_alloc_heap, 0));

      //our memory bar
      let memory_used = ((msg.heap_size / (msg.heap_size + msg.free_heap)) * 100).toFixed(2); //esp32-s3 512kb ram
      $(`#memory_usage div`).css("width", memory_used + "%");
      $(`#memory_usage div`).html(YB.Util.formatBytes(msg.heap_size, 0));
      $(`#memory_usage`).attr("aria-valuenow", memory_used);

      //wifi rssi.
      if (msg.rssi) {
        $("#rssi").html(msg.rssi + "dBm");
        $(".rssi_icon").hide();
        let rssi_icon = YB.Util.getWifiIconForRSSI(msg.rssi);
        $(`#${rssi_icon}`).show();
      }
      $("#uuid").html(msg.uuid);
      $("#ip_address").html(msg.ip_address);

      //our mqtt?
      if (YB.App.config.enable_mqtt) {
        if (msg.mqtt_connected)
          $("#mqtt_status").html(`<span class="text-success">Connected</span>`);
        else
          $("#mqtt_status").html(`<span class="text-danger">Disconnected</span>`);
      } else
        $("#mqtt_status").html(`<span>Disabled</span>`);

      if (msg.bus_voltage)
        $("#bus_voltage").html(msg.bus_voltage.toFixed(1) + "V");
      else
        $("#bus_voltage_row").remove();

      if (msg.fans)
        $("#fan_rpm").html(msg.fans.map((a) => a.rpm + "RPM").join(", "));
      else
        $("#fan_rpm_row").remove();

      YB.App.pageReady.stats = true;
    },

    handleFullConfigMessage: function (msg) {
      //YB.log("full config");

      //load our config
      const textarea = document.getElementById('configurationTextarea');
      const prettyJSON = JSON.stringify(msg.config, null, 2); // 2-space indentation
      textarea.value = prettyJSON;

      //save handler
      document.getElementById('configurationSave').addEventListener('click', () => {
        const configText = document.getElementById('configurationTextarea').value;

        try {
          // Try to parse the JSON
          const parsed = JSON.parse(configText);

          // Re-serialize it compactly (no whitespace)
          const compact = JSON.stringify(parsed);

          //send it off for saving.
          YB.client.send({
            "cmd": "save_config",
            "config": compact
          }, true);

          //let them know.
          $("#saveIndicator").html("Saved!");

        } catch (err) {
          //show an error
          $("#invalidConfigurationJSON").show();

          //highlight the textarea for feedback
          textarea.style.border = "2px solid red";
          setTimeout(() => textarea.style.border = "", 2000);
        }
      });

      //copy handler
      document.getElementById('configurationCopy').addEventListener('click', () => {
        const text = document.getElementById('configurationTextarea').value;

        if (navigator.clipboard && navigator.clipboard.writeText) {
          navigator.clipboard.writeText(text)
            .then(() => $("#copyIndicator").html("Copied!"))
            .catch(err => console.error("Clipboard write failed:", err));
        } else {
          // Fallback for insecure contexts aka locally hosted non-secure esp32 pages... or old browsers
          textarea.select();
          try {
            document.execCommand("copy");
            $("#copyIndicator").html("Copied!");
          } catch (err) {
            console.error("Fallback copy failed:", err);
          }
        }
      });

      //download handler
      document.getElementById('configurationDownload').addEventListener('click', () => {
        const text = document.getElementById('configurationTextarea').value;
        const blob = new Blob([text], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        const today = new Date();
        const dateStr = today.toISOString().split('T')[0];
        a.download = `${YB.App.config.hostname}_${YB.App.config.uuid}_${dateStr}_config.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url); // cleanup
      });

      YB.App.addConfigurationDragDropHandler();
    },

    handleNetworkConfigMessage: function (msg) {
      //YB.log("network config");

      //YB.log(msg);
      $("#wifi_mode").val(msg.wifi_mode);
      $("#wifi_ssid").val(msg.wifi_ssid);
      $("#wifi_pass").val(msg.wifi_pass);
      $("#local_hostname").val(msg.local_hostname);

      YB.App.pageReady.network = true;
    },

    handleAppConfigMessage: function (msg) {
      //YB.log("app config");

      //update some ui stuff
      YB.App.updateRoleUI();
      YB.App.toggleRolePasswords(msg.default_role);

      //what is our update interval?
      if (msg.app_update_interval)
        YB.App.updateInterval = msg.app_update_interval;

      //for our melodies
      $('#startup_melody').val(msg.startup_melody);

      //YB.log(msg);
      $("#admin_user").val(msg.admin_user);
      $("#admin_pass").val(msg.admin_pass);
      $("#guest_user").val(msg.guest_user);
      $("#guest_pass").val(msg.guest_pass);
      $("#app_update_interval").val(msg.app_update_interval);
      $("#default_role").val(msg.default_role);
      $("#default_role").change(function () { YB.App.toggleRolePasswords($(this).val()) });
      $("#app_enable_mfd").prop("checked", msg.app_enable_mfd);
      $("#app_enable_api").prop("checked", msg.app_enable_api);
      $("#app_enable_serial").prop("checked", msg.app_enable_serial);
      $("#app_enable_ota").prop("checked", msg.app_enable_ota);

      //for our ssl stuff
      $("#app_enable_ssl").prop("checked", msg.app_enable_ssl);
      $("#server_cert").val(msg.server_cert);
      $("#server_key").val(msg.server_key);

      //hide/show these guys
      if (msg.app_enable_ssl) {
        $("#server_cert_container").show();
        $("#server_key_container").show();
      }

      //make it dynamic too
      $("#app_enable_ssl").change(function () {
        if ($("#app_enable_ssl").prop("checked")) {
          $("#server_cert_container").show();
          $("#server_key_container").show();
        }
        else {
          $("#server_cert_container").hide();
          $("#server_key_container").hide();
        }
      });

      //for our mqtt stuff
      $("#app_enable_mqtt").prop("checked", msg.app_enable_mqtt);
      $("#app_enable_ha_integration").prop("checked", msg.app_enable_ha_integration);
      $("#app_use_hostname_as_mqtt_uuid").prop("checked", msg.app_use_hostname_as_mqtt_uuid);
      $("#mqtt_server").val(msg.mqtt_server);
      $("#mqtt_user").val(msg.mqtt_user);
      $("#mqtt_pass").val(msg.mqtt_pass);
      $("#mqtt_cert").val(msg.mqtt_cert);

      //hide/show these guys
      if (msg.app_enable_mqtt) {
        $(".mqtt_field").show();
      }

      //make it dynamic too
      $("#app_enable_mqtt").change(function () {
        if ($("#app_enable_mqtt").prop("checked")) {
          $(".mqtt_field").show();
        }
        else {
          $("#app_enable_ha_integration").prop("checked", false);
          $(".mqtt_field").hide();
        }
      });

      YB.App.pageReady.settings = true;
    },

    handleOTAProgressMessage: function (msg) {
      //YB.log("ota progress");

      //OTA is blocking... so update our heartbeat
      //TODO: do we need to update this?
      //last_heartbeat = Date.now();

      let progress = Math.round(msg.progress);

      let prog_id = `#firmware_progress`;
      $(prog_id).css("width", progress + "%").text(progress + "%");
      if (progress == 100) {
        $(prog_id).removeClass("progress-bar-animated");
        $(prog_id).removeClass("progress-bar-striped");
      }

      //was that the last?
      if (progress == 100) {
        YB.App.showAlert("Firmware update successful.", "success");

        //reload our page
        setTimeout(function () {
          location.reload(true);
        }, 2500);
      }
    },

    handleErrorMessage: function (msg) {
      //did we turn login off?
      if (msg.message == "Login not required.") {
        Cookies.remove("username");
        Cookies.remove("password");
      }

      //keep the u gotta login to the login page.
      if (msg.message == "You must be logged in.")
        YB.App.openPage("login");
      else
        YB.App.showAlert(msg.message);
    },

    handleLoginMessage: function (msg) {

      //keep the login success stuff on the login page.
      if (msg.message == "Login successful.") {
        //once we know our role, we can load our other configs.
        YB.App.role = msg.role;

        // YB.log(`YB.App.role: ${YB.App.role}`);
        // YB.log(`YB.App.currentPage: ${YB.App.currentPage}`);

        //only needed for login page, otherwise its autologin
        if (YB.App.currentPage == "login") {
          //save user/pass to cookies.
          if (YB.App.username && YB.App.password) {
            Cookies.set('username', YB.App.username, { expires: 365 });
            Cookies.set('password', YB.App.password, { expires: 365 });
          }
        }

        //prep the site
        YB.App.updateRoleUI();
        YB.App.loadConfigs();
        YB.App.openDefaultPage();
      }
      else
        YB.App.showAlert(msg.message, "success");
    },

    handleSetThemeMessage: function (msg) {
      //light/dark mode
      YB.App.setTheme(msg.theme);
    },

    handleSetBrightnessMessage: function (msg) {
      //did we get brightness?
      if (msg.brightness && !YB.App.currentlyPickingBrightness)
        $('#brightnessSlider').val(Math.round(msg.brightness * 100));
    },

  };

  //setup all of our message handlers.
  YB.App.addMessageHandler("hello", YB.App.handleHelloMessage);
  YB.App.addMessageHandler("status", YB.App.handleStatusMessage);
  YB.App.addMessageHandler("config", YB.App.handleConfigMessage);
  YB.App.addMessageHandler("update", YB.App.handleUpdateMessage);
  YB.App.addMessageHandler("stats", YB.App.handleStatsMessage);
  YB.App.addMessageHandler("full_config", YB.App.handleFullConfigMessage);
  YB.App.addMessageHandler("network_config", YB.App.handleNetworkConfigMessage);
  YB.App.addMessageHandler("app_config", YB.App.handleAppConfigMessage);
  YB.App.addMessageHandler("ota_progress", YB.App.handleOTAProgressMessage);
  YB.App.addMessageHandler("error", YB.App.handleErrorMessage);
  YB.App.addMessageHandler("login", YB.App.handleLoginMessage);
  YB.App.addMessageHandler("set_theme", YB.App.handleSetThemeMessage);
  YB.App.addMessageHandler("set_brightness", YB.App.handleSetBrightnessMessage);

  // expose to global
  global.YB = YB;
})(this);