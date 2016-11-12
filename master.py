#!/usr/bin/env python

## Credit to https://oscarliang.com/raspberry-pi-arduino-connected-i2c/

# Potential NumberToArduino values
# 0 = Pi not present (used on Arduino)
PiAlive = 2
PiShuttingDown = 3

# Let's assume all is well.
NumberFromArduino = 1
ShuttingDown = 0

import smbus
import time
import os

# for RPI version 1, use "bus = smbus.SMBus(0)"
bus = smbus.SMBus(1)

# This is the address we setup in the Arduino Program
address = 0x04

def writeNumber(value):
    bus.write_byte(address, value)
    # bus.write_byte_data(address, 0, value)
    return -1

def readNumber():
    number = bus.read_byte(address)
    print "Recieved :", number
    # number = bus.read_byte_data(address, 1)
    return number

while True:
    # FIXME Needs to handle cancellations, and repeat messaging

    if NumberFromArduino == ShuttingDown:
      if ShuttingDown != 1:
        print "Power changed to PowerFail"
        # os.system('/sbin/shutdown --poweroff -t 60')
      if ShuttingDown == 1:
        print "Waiting for shutdown"

      # Tell the Arduino the Pi is shutting down
      NumberToArduino = PiShuttingDown

    elif NumberFromArduino == 1:
      print "Power is OK"
      # Tell the Arduino the Pi is up
      NumberToArduino = PiAlive

    try:
      NumberFromArduino = readNumber()
      # Send status to Arduino
      writeNumber(NumberToArduino)
      print "Sent Arduino :", NumberToArduino

    except IOError:
      print "I2C slave not present"

    time.sleep(1)
