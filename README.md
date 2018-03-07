# KilnController
An ESP32 based glass kiln controller, using the ESP-WROOM-32 chipset on a WeMos 128x64 OLED module.  

We have several kilns, from older ceramic ones to large coffin kilns to small 120VAC desktop kilns. Most of them have been upgraded or hacked with newer Paragon controllers, but some of the smaller desktop ones are still manual. Since we teach classes, we need to be able to use the kiln in a classroom or take it with us somewhere and teach while it fires.

This is a work in progress.. 

Initally I'd like the folowing features:
K-Type Thermocouple, with a 1300C limit for sensing: (Glass firing rarely goes that high, but its a minor added expense to support and it would allow more ceramic uses.)
The major electronics components would be a 25A 240V SSD, buzzer, pushbutton (for reset), the ESP32 Wemos module, MAX13885 module, Thermocouple, small 5V, 1A switching module, and various resistors/caps/mosfets to drive and interface the module to the kiln.
PID temperature control - note that with a SSD a slower PWM can be used to control elements.
Toggle switch to defeat all element power.
Web interface to control and monitor the system remotely.
Ability to connect to WiFi or local AP mode when Wifi is unavailible or needs to be set up.
HTML basic authentication.

Optionally:
An opto-isolated sense on the high voltage side of the SSD to ensure SSD failures are caught.
An android app, possibly low power BT connected instead of WiFi.
SSL protection of more secure authentication methods.

I *might* put circuit diagrams up here but I'm really just intending to use the github location to store ESP32 code. This started early 2018, and its difficult to find all the pieces i need on the net and put them together, so I thought I'd post what i have put together for others to find and use.

Most likely, once I get a functional system I will stop design work and just run with it.. this is more of a personal project, and while I appreciate questions, I'm not really intending to publicly support this. Just share the code, basically.

I borrow heavily from:
espressif's website
ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
Tech Tutorials (https://techtutorialsx.com/2017/12/17/esp32-arduino-http-server-getting-query-parameters/)
Arduino Forums (http://forums.arduino.cc) (Watch out for the tro(pau)llS! :) )

