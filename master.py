#!/usr/bin/env python

## Credit to https://oscarliang.com/raspberry-pi-arduino-connected-i2c/

import logging
import smbus
import time
import os

Debug = 1

# Potential NumberToArduino values
# 0 = Pi not present (used on Arduino)
PiAlive = 2
PiShuttingDown = 3

# Let's assume all is well.
NumberFromArduino = 1
ShuttingDown = 0
ShutdownCalled = 0
LogFile = /var/log/jeepower-master.log
LogLevels = {'debug': logging.DEBUG,
          'info': logging.INFO,
          'warning': logging.WARNING,
          'error': logging.ERROR,
          'critical': logging.CRITICAL}

if Debug == 0:
    logging.basicConfig(filename=LOGFILE, level=logging.INFO)
if Debug == 1:
    logging.basicConfig(filename=LOGFILE, level=logging.DEBUG)

logging.info('Starting i2c master daemon')
logging.info('INFO MODE')
logging.debug('DEBUG MODE')

# for RPI version 1, use "bus = smbus.SMBus(0)"
bus = smbus.SMBus(1)

# This is the address we setup in the Arduino Program
address = 0x04

def writeNumber(value):
    logging.debug("Writing %s to %s" % (value, address))
    bus.write_byte(address, value)
    # bus.write_byte_data(address, 0, value)
    return -1

def readNumber():
    number = bus.read_byte(address)
    logging.debug("Received %s from %s" % (number, address))
    print "Recieved :", number
    # number = bus.read_byte_data(address, 1)
    return number

while True:
    # FIXME Needs to handle cancellations, and repeat messaging

    if NumberFromArduino == ShuttingDown:
      logging.info("Power has failed")
      if ShutdownCalled != 1:
        print "Power changed to PowerFail"
        logging.info("Calling for poweroff with 60 second timer")
        os.system('/sbin/shutdown --poweroff -t 60')
        ShutdownCalled = 1
      if ShutdownCalled == 1:
        logging.debug("Waiting for shutdown")
        print "Waiting for shutdown"

      # Tell the Arduino the Pi is shutting down
      NumberToArduino = PiShuttingDown

    elif NumberFromArduino == 1:
      logging.info("Power is OK")
      print "Power is OK"
      # Tell the Arduino the Pi is up
      NumberToArduino = PiAlive
      if ShutdownCalled == 1:
        logging.info("Cancelling running shutdown")
        # Cancel the running shutdown
        os.system('/sbin/shutdown -c')
        ShutdownCalled = 0

    try:
      NumberFromArduino = readNumber()
      # Send status to Arduino
      writeNumber(NumberToArduino)
      print "Sent Arduino :", NumberToArduino

    except IOError:
      logging.info("I2C slave not present. Check Arduino and communications")
      print "I2C slave not present"

    time.sleep(1)
