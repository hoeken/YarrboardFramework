(function (global) { // private scope
  // work in the global YB namespace.
  var YB = global.YB || {};

  // Page constructor function
  YB.Page = function (config) {
    // Initialize properties with defaults
    this.name = config.name || null;
    this.displayName = config.displayName || config.name || null;
    this.permissionLevel = config.permissionLevel || "admin";
    this.showInNavbar = config.showInNavbar !== undefined ? config.showInNavbar : true;
    this.ready = config.ready !== undefined ? config.ready : false;
    this.content = config.content !== undefined ? config.content : null;
    this.position = config.position !== undefined ? config.position : "last";

    this.openCallbacks = [];
    this.closeCallbacks = [];

    // Store reference to content div and navbar entry if they already exist
    this._contentDiv = null;
    this._navbarEntry = null;
  };

  YB.Page.prototype.setup = function () {
    //add navbar entry
    if (this.showInNavbar)
      this.addNavbarEntry();

    //setup our content div
    if (this.content)
      this.setContent(this.content);
  };

  YB.Page.prototype.onOpen = function (callback) {
    this.openCallbacks.push(callback);
  };

  YB.Page.prototype.open = function () {

    if (!this.allowed(YB.App.role)) {
      YB.log(`${page} not allowed for ${YB.App.role}`);
      return;
    }

    //update our nav - remove active from all nav links first
    $('#navbarLinks .nav-link').removeClass("active");
    $(`#${this.name}Nav a`).addClass("active");

    //hide all pages, we will show this one when its ready.
    $("div.pageContainer").hide();

    //run our start callbacks
    for (let cb of this.openCallbacks)
      cb(this);

    //waits until page is ready to open.
    this.openWhenReady();
  };

  YB.Page.prototype.onClose = function (callback) {
    this.closeCallbacks.push(callback);
  };

  YB.Page.prototype.close = function (callback) {
    for (let cb of this.closeCallbacks)
      cb(this);
  };

  YB.Page.prototype.openWhenReady = function () {
    //are we ready yet?
    if (this.ready) {
      $("#loading").hide();
      $(`#${this.name}Page`).show();
    }
    else {
      $("#loading").show();
      setTimeout(() => this.openWhenReady(), 100);
    }
  };

  // Add a content div to the page
  YB.Page.prototype.addContentDiv = function () {
    if (this._contentDiv) {
      return this._contentDiv;
    }

    if (!this.name) {
      console.error("Page must have a name to create content div");
      return null;
    }

    // Create the content div
    var contentDiv = $('<div></div>')
      .attr('id', this.name + 'Page')
      .addClass('pageContainer')
      .css('display', 'none');

    // Append to main content area (after liveAlertPlaceholder)
    $('#liveAlertPlaceholder').after(contentDiv);

    this._contentDiv = contentDiv;
    return contentDiv;
  };

  // Get the content div for this page
  YB.Page.prototype.getContentDiv = function () {
    if (this._contentDiv) {
      return this._contentDiv;
    }

    if (!this.name) {
      return null;
    }

    // Try to find existing div
    var contentDiv = $('#' + this.name + 'Page');
    if (contentDiv.length > 0) {
      this._contentDiv = contentDiv;
      return contentDiv;
    }

    return null;
  };

  // Create a navbar entry for this page
  YB.Page.prototype.createNavbarEntry = function () {
    if (this._navbarEntry) {
      return this._navbarEntry;
    }

    if (!this.name || !this.showInNavbar) {
      return null;
    }

    // Create the navbar list item
    var navItem = $('<li></li>')
      .attr('id', this.name + 'Nav')
      .addClass('nav-item nav-permission');

    // Create the link
    var navLink = $('<a></a>')
      .addClass('nav-link')
      .attr('href', '#' + this.name)
      .attr('onclick', 'YB.App.openPage(\'' + this.name + '\')')
      .text(this.displayName);

    navItem.append(navLink);

    this._navbarEntry = navItem;
    return navItem;
  };

  // Add the navbar entry to the DOM
  YB.Page.prototype.addNavbarEntry = function () {
    let navEntry = this.createNavbarEntry();
    let navbarNav = $('#navbarLinks');

    if (this.position === 'first') {
      // Add as the first entry
      navbarNav.prepend(navEntry);
    } else if (typeof this.position === 'string') {
      // Add after the specified element with id #{position}Nav
      var targetNav = $('#' + this.position + 'Nav');
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

  // Get the navbar entry for this page
  YB.Page.prototype.getNavbarEntry = function () {
    if (this._navbarEntry) {
      return this._navbarEntry;
    }

    if (!this.name) {
      return null;
    }

    // Try to find existing navbar entry
    var navEntry = $('#' + this.name + 'Nav');
    if (navEntry.length > 0) {
      this._navbarEntry = navEntry;
      return navEntry;
    }

    return null;
  };

  // Set the content of the page
  YB.Page.prototype.setContent = function (content) {
    var contentDiv = this.getContentDiv();
    if (!contentDiv) {
      contentDiv = this.addContentDiv();
    }

    if (contentDiv) {
      contentDiv.html(content);
    }
  };

  // Show the page
  YB.Page.prototype.show = function () {
    var contentDiv = this.getContentDiv();
    if (contentDiv) {
      contentDiv.show();
    }
  };

  // Hide the page
  YB.Page.prototype.hide = function () {
    var contentDiv = this.getContentDiv();
    if (contentDiv) {
      contentDiv.hide();
    }
  };

  // Remove the page completely
  YB.Page.prototype.remove = function () {
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

  // Check if a user role has permission to access this page
  YB.Page.prototype.allowed = function (role) {
    // Define role hierarchy
    var roleLevel = {
      'nobody': 1,
      'guest': 2,
      'admin': 3
    };

    var requiredLevel = roleLevel[this.permissionLevel] || 2;
    var userLevel = roleLevel[role] || 0;

    return userLevel >= requiredLevel;
  };

  // expose to global
  global.YB = YB;
})(this);
