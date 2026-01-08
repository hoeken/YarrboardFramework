(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  // SettingsPanel constructor function
  YB.SettingsPanel = function (config) {
    // Initialize properties with defaults
    this.name = config.name || null;
    this.displayName = config.displayName || config.name || null;
    this.content = config.content !== undefined ? config.content : null;
    this.position = config.position !== undefined ? config.position : "last";
    this.useTitle = config.useTitle !== undefined ? config.useTitle : true;
    this.roundedBorder = config.roundedBorder !== undefined ? config.roundedBorder : true;

    this.openCallbacks = [];
    this.closeCallbacks = [];

    // Store reference to content div and navbar entry
    this._contentDiv = null;
    this._navbarEntry = null;
  };

  YB.SettingsPanel.prototype.setup = function () {

    this.addNavbarEntry();

    //setup our content div
    if (this.content)
      this.setContent(this.content);
  };

  YB.SettingsPanel.prototype.onOpen = function (callback) {
    this.openCallbacks.push(callback);
  };

  YB.SettingsPanel.prototype.open = function () {
    //update our nav - remove active from all nav links first
    $('#navbarSettingsLinks .nav-link').removeClass("active");
    $(`#${this.name}SettingsNav a`).addClass("active");

    //hide all panels, then show this one
    $("div.settingsContainer").hide();

    //show the panel
    $(`#${this.name}SettingsPanel`).show();

    //run our start callbacks
    for (let cb of this.openCallbacks)
      cb(this);
  };

  YB.SettingsPanel.prototype.onClose = function (callback) {
    this.closeCallbacks.push(callback);
  };

  YB.SettingsPanel.prototype.close = function (callback) {
    for (let cb of this.closeCallbacks)
      cb(this);
  };

  // Add a content div to the page
  YB.SettingsPanel.prototype.addContentDiv = function () {
    if (this._contentDiv) {
      return this._contentDiv;
    }

    if (!this.name) {
      console.error("Settings Panel must have a name to create content div");
      return null;
    }

    let extra_classes = "";
    if (this.roundedBorder)
      extra_classes += "p-3 border border-secondary rounded";

    var contentDiv = $( /* html */ `
      <div id="${this.name}SettingsPanel" class="settingsContainer mb-3 ${extra_classes}" style="display: none"></div>
    `);

    // Append to main content area
    $('#settingsContainer').append(contentDiv);

    this._contentDiv = contentDiv;
    return contentDiv;
  };

  // Get the content div for this SettingsPanel
  YB.SettingsPanel.prototype.getContentDiv = function () {
    if (this._contentDiv) {
      return this._contentDiv;
    }

    if (!this.name) {
      return null;
    }

    // Try to find existing div
    var contentDiv = $('#' + this.name + 'SettingsPanel');
    if (contentDiv.length > 0) {
      this._contentDiv = contentDiv;
      return contentDiv;
    }

    return null;
  };

  // Create a navbar entry for this SettingsPanel
  YB.SettingsPanel.prototype.createNavbarEntry = function () {
    if (this._navbarEntry) {
      return this._navbarEntry;
    }

    if (!this.name) {
      return null;
    }

    // Create the navbar list item and link
    var navItem = $(`
      <li id="${this.name}SettingsNav" class="nav-item nav-permission text-center text-sm-start">
        <a class="nav-link" onclick="YB.App.openSettingsPanel('${this.name}')">
          ${this.displayName}
        </a>
      </li>
    `);

    this._navbarEntry = navItem;
    return navItem;
  };

  // Add the navbar entry to the DOM
  YB.SettingsPanel.prototype.addNavbarEntry = function () {
    let navEntry = this.createNavbarEntry();
    let navbarNav = $('#navbarSettingsLinks');

    if (this.position === 'first') {
      // Add as the first entry
      navbarNav.prepend(navEntry);
    } else if (typeof this.position === 'string') {
      // Add after the specified element with id #{position}Nav
      var targetNav = $('#' + this.position + 'SettingsNav');
      if (targetNav.length > 0) {
        targetNav.after(navEntry);
      } else {
        // If target not found, append at end
        navbarNav.append(navEntry);
      }
    } else {
      // position is last or null, add as last entry
      navbarNav.append(navEntry);
    }
  };

  // Get the navbar entry for this SettingsPanel
  YB.SettingsPanel.prototype.getNavbarEntry = function () {
    if (this._navbarEntry) {
      return this._navbarEntry;
    }

    if (!this.name) {
      return null;
    }

    // Try to find existing navbar entry
    var navEntry = $('#' + this.name + 'SettingsNav');
    if (navEntry.length > 0) {
      this._navbarEntry = navEntry;
      return navEntry;
    }

    return null;
  };

  // Set the content of the SettingsPanel
  YB.SettingsPanel.prototype.setContent = function (content) {
    var contentDiv = this.getContentDiv();
    if (!contentDiv) {
      contentDiv = this.addContentDiv();
    }

    if (contentDiv) {
      if (this.useTitle)
        contentDiv.html(`<h5>${this.displayName}</h5>${content}`);
      else
        contentDiv.html(content);
    }
  };

  // Show the SettingsPanel
  YB.SettingsPanel.prototype.show = function () {
    var contentDiv = this.getContentDiv();
    if (contentDiv) {
      contentDiv.show();
    }
  };

  // Hide the SettingsPanel
  YB.SettingsPanel.prototype.hide = function () {
    var contentDiv = this.getContentDiv();
    if (contentDiv) {
      contentDiv.hide();
    }
  };

  // Remove the SettingsPanel completely
  YB.SettingsPanel.prototype.remove = function () {
    var contentDiv = this.getContentDiv();
    if (contentDiv) {
      contentDiv.remove();
      this._contentDiv = null;
    }

    var navEntry = this.getNavbarEntry();
    if (navEntry) {
      navEntry.remove();
      this._navbarEntry = null;
    }
  };

  // expose to global
  global.YB = YB;
})(this);
