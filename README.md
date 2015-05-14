# ArduinoOcclusion
The aim of this small project is to control PLATO occlusion googles ( http://www.translucent.ca/ ) with an Arduino Uno and in addition get the ability to transmit and control the status over Ethernet (e.g. to a driving simulator or smartphone).


more information, pictures and videos: http://www.lfe.mw.tum.de/en/open-source/arduino-occlusion/

Operation/function:
The Arduino is connected to the logic input of the wide spread PLATO glasses (western plug) with the Arduino Pins 8 and 9. These control, if the left/right glass of the glasses should close/open.

Pin 1 is intended to connect e.g. a TIP 122 transistor, which switches an electroluminescent inverter on/off. If you connect a PDLC film to this inverter you have DIY occlusion glasses.

The LED on Pin3 shows the current state open/close.
The LED on Pin7 shows if an experiment is currently in progress/running

A button on Pin2 can start and stop the experiment.
Another button on Pin6 can toggle the state occluded/open, if no experiment is in progress.

Transmission:
If no experiment is in progress, the Arduino transmits over serial and Ethernet every second an ‘R’ (for ‘R’eady). This prevents timeouts on some socket implementations. At the moment an experiment starts, the Arduino sends ‘#’. During the experiment the Arduino transmits every time when the state changes an ‘o’ (open) or ‘c’ (close). When the experiment is ended the Arduino sends ‘$’. At the end of the experiment, the experimental stats (Total Task Time and Total Shutter Open Time) are output via serial line.

To control the Arduino remote via serial or Ethernet a simple ‘t’ (toggle) can switch between open/close, if no experiment is in progress. A ‘#’ starts an experiment and a ‘$’ stops.

A simple socket connection needs to be established for remote control, e.g., via telnet, a telnet App on a smartphone, a driving simulator socket, or the many socket implementations in nearly all measurement and programming languages.

To modify IP and port look for the lines:
IPAddress gIp(192,168,1,18);
EthernetServer gServer(7009);

-----------
[![DOI](https://zenodo.org/badge/12944/InstituteOfErgonomics/ArduinoOcclusion.svg)](http://dx.doi.org/10.5281/zenodo.17616)
