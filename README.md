[![Build Status](https://travis-ci.org/kylegordon/JeePower2.png?branch=master)](https://travis-ci.org/kylegordon/JeePower2)

JeePower2
Kyle Gordon <kyle@lodge.glasgownet.com>
Available under the terms of the GPL v3

A simple little Arduino compatible sketch used to provide some intelligence to an in-car power supply.

WIRING
------

There are a variety of components in use here. A Jeenode flavour of Arduino, with the Opto-Coupler 'plug' which simply links a couple of opto-couplers as inputs to the Arduino, and a Output Relay 'plug' which is provides two small relays to interface to the Arduino.

Along with the Arduino, there is a buck converter that operates from an input supply voltage of 4.5 to 28V ( supplying up to 3A ) ( https://www.aliexpress.com/item/5-pcs-Ultra-Small-Size-DC-DC-Step-Down-Power-Supply-Module-3A-Adjustable-Buck-Converter/32261885063.html ), a neat Li-Ion battery charger ( http://www.banggood.com/37V-Liion-Battery-Mini-USB-To-USB-A-Power-Apply-Module-p-928948.html?rmmds=myorder ) that has the ability to charge and supply at the same time, and a Raspberry Pi Zero.

The entire system operates off of a 5v supply. The incoming car 12v is fed from the ignition, and goes straight into the buck converter. This converter is adjusted to output 5V, which then feeds into both the Li-Ion battery charger and the opto-coupler inputs on the Arduino.

The battery charger output goes to permanently power the Arduino, and the positive side of the Arduino relay. The switched side of the Arduino relay goes to the power input of the Raspberry Pi. This way, the Arduino gets to control the power supply of the Raspberry Pi.

The Raspberry Pi and the Arduino communicate using I2C over a 3 wire link between pins 3 ( data ) and 5 ( clock ) on the Pi, and the SDA ( data ) and SCL ( clock ) pins on the Arduino.

BUILDING
--------

This is all done with PlatformIO. To build, simply run *pio run*

UPLOADING
---------

Yet more PlatformIO magic - *pio run -target upload --upload-port /dev/ttyUSB0*

BASIC OPERATION
---------------

The Arduino sits and monitors the Opto-Coupler inputs to ascertain the state of available power. At the same time, it operates as an I2C slave to the Raspberry Pi. I2C slave functionality only came to the Linux Kernal recently, so hence the Pi is the master, and the Arduino is the slave. Every second or so the Pi asks for a status code, which the Arduino considers to be a heartbeat. The Arduino responds with a code describing the power state.

A master I2C script runs on the Pi, having been started by systemd, and performs actions based on those return codes. Most of the time the action is to start or cancel a shutdown process.
