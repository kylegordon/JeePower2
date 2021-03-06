[![Build Status](https://travis-ci.org/kylegordon/JeePower2.png?branch=master)](https://travis-ci.org/kylegordon/JeePower2)

JeePower2
Kyle Gordon <kyle@lodge.glasgownet.com>
Available under the terms of the GPL v3

A simple little Arduino compatible sketch used to provide some intelligence to an in-car power supply.

It monitors vehicle ignition and oil pressure. Only when the oil pressure warning light hsa been extinguished and the ignition has come on will it start some timers and eventually power up an output relay.

If the ignition is off, it will run a countdown timer before turning off a relay that's used to indicate an impending loss of power. After another timer has expired, it will kill the main power.

The current use for the indication relay is to indicate to a Bifferboard over the GPIO pins that the power will be lost. The Bifferboard does not support ACPI, so another method had to be found. A python script monitors the state of the input, and will initiate a shutdown if the input is brought low. If the Arduino lowers this input, and then the ignition is brought back on, the Arduino will continue the shut down process and then restart the whole system when appropriate, as there is no way to interrupt the shutdown once it's started on the Bifferboard.

BUILDING
--------

Make sure you have avr-libc and avrdude installed, along with the latest copy of the jeelib libraries from https://github.com/jcw/jeelib

Using scons, in the downloaded directory, execute the following whilst substituting in the correct parameters for your environment.

scons ARDUINO_HOME=~/Applications/arduino/ ARDUINO_BOARD=uno

UPLOADING
---------

As above, but append 'upload' to the end

scons ARDUINO_HOME=~/Applications/arduino/ ARDUINO_BOARD=uno upload

REWRITE IDEA
------------

Signal line from CPU to MCU to indicate current power state. (CPUState)
Signal line from MCU to CPU to indicate desired power state. 
MCU control of CPU power line.

Rough pseudocode...

if IgnitionState = on
  Cancel shutdown timer
  if CPUState = off
    Sleep enough to overcome delay between OS turning off signal, and halting
    Raise MCU to CPU line
    Raise PowerSupplyState
  elseif CPUState = on
    pass
  fi
fi
if IgnitionState = off
  If ShutdownTimer = 0
    Start ShutdownTimer
  elseif ShutdownTimer > 30 seconds
    Lower MCU to CPU line to signal shutdown
  elseif ShutdownTimer > 120 seconds
    Lower PowerSupplyState (to turn CPU off)
    Go to Sleep
  fi
fi
