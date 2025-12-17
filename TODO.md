## v1.0.0 Release

* Fix default values - #defines arent good enough.

* fix our CI hooks

* modify the gulp.js script to pull from the framework lib directory
* project specific gulp rules to pull in custom stuff logo, css, js, etc.
    * html/index.html -> overrides framework html
    * html/logo.png -> overrides framework.html
    * html/css/* -> gets added to the framework css includes
    * html/js/* -> gets added to the framework js includes

* figure out how to manage BaseChannel mqtt calls
  * move all the mqtt stuff to ChannelController
  * move all the templated calls from BaseChannel to ChannelController