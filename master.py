#!/usr/bin/env python

## Credit to https://oscarliang.com/raspberry-pi-arduino-connected-i2c/

# Potential NumberToArduino values
# 0 = Pi not present (used on Arduino)
# 1 = Pi shutting down
# 2 = Pi alive

import smbus
import time
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
    # number = bus.read_byte_data(address, 1)
    return number

while True:

    # A 2 to say that the Pi is alive
    NumberToArduino = 2


    # Send the contents of var
    writeNumber(NumberToArduino)

    print "RPI: Hi Arduino, I sent you ", NumberToArduino
    # sleep one second
    time.sleep(1)

    NumberFromArduino = readNumber()
    print "Arduino: Hey RPI, I received a digit ", NumberFromArduino
    print
