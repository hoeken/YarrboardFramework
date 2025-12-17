## v1.0.0 Release

* convert frothfet
* convert brineomatic

* figure out how to manage BaseChannel mqtt calls
  * move all the mqtt stuff to ChannelController
  * move all the templated calls from BaseChannel to ChannelController

* modify the gulp.js script to pull from the framework lib directory
  * it should also write the files to the framework lib directory for inclusion
* project specific gulp rules to pull in custom stuff logo, css, js, etc.
    * html/index.html -> overrides framework html
    * html/logo.png -> overrides framework.html
    * html/css/* -> gets added to the framework css includes
    * html/js/* -> gets added to the framework js includes

* fix our CI hooks