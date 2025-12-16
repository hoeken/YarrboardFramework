## v1.0.0 Release

* fix our CI hooks
* start the conversion of firmware with Sendit as the first target

* modify the gulp.js script to pull from the framework lib directory
* project specific gulp rules to pull in custom stuff logo, css, js, etc.
    * html/index.html -> overrides framework html
    * html/logo.png -> overrides framework.html
    * html/css/* -> gets added to the framework css includes
    * html/js/* -> gets added to the framework js includes

* protocol hooks
  * move the controller specific commands to each controllers setup.
  * generateUpdateHook(json) - generate update json
  * generateFastUpdateHook(callback) - generate fast update json
  * generateStatsHook(callback) - generate stats json

  * onMessage() - gets called for every single message
    * return true = continue
    * return false = stop

* figure out how to manage BaseChannel mqtt calls
  * move all the mqtt stuff to ChannelController
  * move all the templated calls from BaseChannel to ChannelController

* investigate how to implement ChannelControllers in a way that is extensible
* copy the ChannelRegistry style from JS and port to c++
  * might make more sense to implement it as ChannelController
  * move channel headers/classes to a channels/ folder
  * rename foo_channel.cpp to FooChannel.cpp to match class names
  * create callbacks:
      * onSetup() - main.cpp
      * onLoop() - main.cpp
      * onInit() - prefs.cpp
    * might be best at channel controller level?
      * onGenerateConfig() - prefs.cpp
      * onLoadConfig() - prefs.cpp
      * onMQTTUpdate() - mqtt.cpp
      * onHADiscovery() - mqtt.cpp
    * for looping only stuff it would be best to do something like this:
        * yba.registerChannels(channels);
        * yba.registerMQTTChannels(channels);
        * yba.registerHAChannels(channels);
