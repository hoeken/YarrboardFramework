(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  // Channel registry + factory under YB
  YB.Util = {

    formatAmpHours: function (aH) {
      //low amp hours?
      if (aH < 10)
        return aH.toFixed(3) + "&nbsp;Ah";
      else if (aH < 100)
        return aH.toFixed(2) + "&nbsp;Ah";
      else if (aH < 1000)
        return aH.toFixed(1) + "&nbsp;Ah";

      //okay, now its kilo time
      aH = aH / 1000;
      if (aH < 10)
        return aH.toFixed(3) + "&nbsp;kAh";
      else if (aH < 100)
        return aH.toFixed(2) + "&nbsp;kAh";
      else if (aH < 1000)
        return aH.toFixed(1) + "&nbsp;kAh";

      //okay, now its mega time
      aH = aH / 1000;
      if (aH < 10)
        return aH.toFixed(3) + "&nbsp;MAh";
      else if (aH < 100)
        return aH.toFixed(2) + "&nbsp;MAh";
      else if (aH < 1000)
        return aH.toFixed(1) + "&nbsp;MAh";
      else
        return Math.round(aH) + "&nbsp;MAh";
    },

    formatWattHours: function (wH) {
      //low watt hours?
      if (wH < 10)
        return wH.toFixed(3) + "&nbsp;Wh";
      else if (wH < 100)
        return wH.toFixed(2) + "&nbsp;Wh";
      else if (wH < 1000)
        return wH.toFixed(1) + "&nbsp;Wh";

      //okay, now its kilo time
      wH = wH / 1000;
      if (wH < 10)
        return wH.toFixed(3) + "&nbsp;kWh";
      else if (wH < 100)
        return wH.toFixed(2) + "&nbsp;kWh";
      else if (wH < 1000)
        return wH.toFixed(1) + "&nbsp;kWh";

      //okay, now its mega time
      wH = wH / 1000;
      if (wH < 10)
        return wH.toFixed(3) + "&nbsp;MWh";
      else if (wH < 100)
        return wH.toFixed(2) + "&nbsp;MWh";
      else if (wH < 1000)
        return wH.toFixed(1) + "&nbsp;MWh";
      else
        return Math.round(wH) + "&nbsp;MWh";
    },

    formatBytes: function (bytes, decimals = 1) {
      if (!+bytes) return '0 Bytes'

      const k = 1024
      const dm = decimals < 0 ? 0 : decimals
      const sizes = ['Bytes', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB']

      const i = Math.floor(Math.log(bytes) / Math.log(k))

      return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`
    },

    formatNumber: function (num, decimals) {
      return Number(num).toLocaleString("en-US", {
        minimumFractionDigits: decimals,
        maximumFractionDigits: decimals
      });
    },

    /**
     * Converts seconds to a human-readable string (Days, Hours, Minutes, Seconds).
     * @param {number|string} seconds - The total seconds to convert.
     * @param {number} [details=2] - The number of significant units to display.
     * 1 = "1 day"
     * 2 = "1 day, 2 hours"
     */
    secondsToDhms: function (seconds, details = 2) {
      seconds = Number(seconds);

      // Calculate all units
      var months = Math.floor(seconds / (3600 * 24 * 30));
      var d = Math.floor(seconds / (3600 * 24));
      var h = Math.floor(seconds % (3600 * 24) / 3600);
      var m = Math.floor(seconds % 3600 / 60);
      var s = Math.floor(seconds % 60);

      // Create an array of the non-zero parts
      var parts = [];

      if (months > 0) parts.push(months + (months == 1 ? " month" : " months"));
      if (d > 0) parts.push(d + (d == 1 ? " day" : " days"));
      if (h > 0) parts.push(h + (h == 1 ? " hour" : " hours"));
      if (m > 0) parts.push(m + (m == 1 ? " minute" : " minutes"));
      if (s > 0) parts.push(s + (s == 1 ? " second" : " seconds"));

      // If the input was 0, return empty string or "0 seconds" depending on preference
      // The original function returned an empty string for 0, preserving that behavior:
      if (parts.length === 0) return "";

      // Slice the array to the requested number of details and join them
      return parts.slice(0, details).join(", ");
    },

    getQueryVariable: function (name) {
      const query = window.location.search.substring(1);
      const vars = query.split("&");
      for (let i = 0; i < vars.length; i++) {
        const pair = vars[i].split("=");
        if (decodeURIComponent(pair[0]) === name) {
          return decodeURIComponent(pair[1] || "");
        }
      }
      return null; // Return null if the variable is not found
    },

    // return true if 'first' is greater than 'second'
    compareVersions: function (first, second) {

      var a = first.split('.');
      var b = second.split('.');

      for (var i = 0; i < a.length; ++i) {
        a[i] = Number(a[i]);
      }
      for (var i = 0; i < b.length; ++i) {
        b[i] = Number(b[i]);
      }
      if (a.length == 2) {
        a[2] = 0;
      }

      if (a[0] > b[0]) return true;
      if (a[0] < b[0]) return false;

      if (a[1] > b[1]) return true;
      if (a[1] < b[1]) return false;

      if (a[2] > b[2]) return true;
      if (a[2] < b[2]) return false;

      return false;
    },

    isCanvasSupported: function () {
      var elem = document.createElement('canvas');
      return !!(elem.getContext && elem.getContext('2d'));
    },

    // Function to update the progress bar
    updateProgressBar: function (ele, progress) {
      // Ensure the progress value is within bounds
      const clampedProgress = Math.round(Math.min(Math.max(progress, 0), 100));

      // Get the progress container and inner progress bar
      const progressContainer = document.getElementById(ele);
      const progressBar = progressContainer.querySelector(".progress-bar");

      if (progressContainer && progressBar) {
        // Update the width style property
        progressBar.style.width = clampedProgress + "%";

        // Update the aria-valuenow attribute for accessibility
        progressContainer.setAttribute("aria-valuenow", clampedProgress);

        // Optional: Update the text inside the progress bar
        progressBar.textContent = clampedProgress + "%";
      } else {
        console.error("Progress bar element not found!");
      }
    },

    getWifiIconForRSSI: function (rssi) {
      let level;

      if (rssi === null || rssi === undefined || rssi <= -100) {
        level = 0; // No signal / disconnected
      } else if (rssi >= -55) {
        level = 4; // Excellent
      } else if (rssi >= -65) {
        level = 3; // Good
      } else if (rssi >= -75) {
        level = 2; // Fair
      } else if (rssi >= -85) {
        level = 1; // Weak
      } else {
        level = 0; // No signal
      }

      return `rssi_icon_${level}`;
    },

    getBootstrapColors: function () {
      const colors = {};
      const styles = getComputedStyle(document.documentElement);

      // List of Bootstrap color variable names
      const colorNames = [
        '--bs-primary',
        '--bs-secondary',
        '--bs-success',
        '--bs-danger',
        '--bs-warning',
        '--bs-info',
        '--bs-light',
        '--bs-dark'
      ];

      // Loop through each variable and store its value without the `--bs-` prefix
      colorNames.forEach(color => {
        const name = color.replace('--bs-', ''); // Remove the prefix
        colors[name] = styles.getPropertyValue(color).trim();
      });

      return colors;
    },

    showFormValidationResults: function (data, errors) {
      //clear our errors
      for (const field of Object.keys(data)) {
        const el = $(`#${field}`);
        el.removeClass("is-valid is-invalid");

        if (errors && errors[field]) {
          //add our invalid class
          el.addClass("is-invalid");

          // Find the right feedback element (works for form-floating and form-switch)
          const msg = errors[field][0];
          const $fb = el.siblings(".invalid-feedback")
            .add(el.closest(".form-check, .form-floating").find(".invalid-feedback"))
            .first();
          $fb.text(msg);
        } else {
          YB.Util.flashClass(el, "is-valid");
        }
      }
    },

    flashClass: function (el, myclass, ms = 3000) {
      el.addClass(myclass);
      setTimeout(() => el.removeClass(myclass), ms);
    },

    humanizeText: function (text) {
      return text
        .toLowerCase()
        .replace(/_/g, " ")
        .replace(/\b\w/g, c => c.toUpperCase());
    },

    populateMelodySelector: function (select) {
      if (YB.App.config.melodies) {
        // select.empty();
        YB.App.config.melodies.forEach(m => {
          $("<option>")
            .val(m)
            .text(YB.Util.humanizeText(m))
            .appendTo(select);
        });

        // Trigger play on change
        select.off("change").on("change", function () {
          const melody = $(this).val();
          if (melody != "NONE") {
            YB.client.send({
              cmd: "play_sound",
              melody: melody
            });
          }
        });
      }
    },
  };

  // expose to global
  global.YB = YB;
})(this);