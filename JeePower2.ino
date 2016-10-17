#include <Arduino.h>

// vim :ts=2:sw=4:sts=4
/*

	by Kyle Gordon <kyle@lodge.glasgownet.com>

http://lodge.glasgownet.com
 */

/*                 JeeNode / JeeNode USB / JeeSMD
						 -------|-----------------------|----|-----------------------|----
						 |       |D3  A1 [Port2]  D5     |    |D3  A0 [port1]  D4     |    |
						 |-------|IRQ AIO +3V GND DIO PWR|    |IRQ AIO +3V GND DIO PWR|    |
						 | D1|TXD|                                           ---- ----     |
						 | A5|SCL|                                       D12|MISO|+3v |    |
						 | A4|SDA|   Atmel Atmega 328                    D13|SCK |MOSI|D11 |
						 |   |PWR|   JeeNode / JeeNode USB / JeeSMD         |RST |GND |    |
						 |   |GND|                                       D8 |BO  |B1  |D9  |
						 | D0|RXD|                                           ---- ----     |
						 |-------|PWR DIO GND +3V AIO IRQ|    |PWR DIO GND +3V AIO IRQ|    |
						 |       |    D6 [Port3]  A2  D3 |    |    D7 [Port4]  A3  D3 |    |
						 -------|-----------------------|----|-----------------------|----
 */


#include <JeeLib.h>
#include <Wire.h>
#include <PortsLCD.h>

//Port optoIn (1);                // Port 1 : Optoisolator inputs
PortI2C myI2C (2);              // Port 2 : I2C driven LCD display for debugging
// Port 3 : Buzzer on DIO and LED on AIO
// Port relays (4);                // Port 4 : Output relays

// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

boolean DEBUG = 1;

// Define the I2C slave address
#define SLAVE_ADDRESS 0x04

LiquidCrystalI2C lcd (myI2C);
#define screen_height 4
#define screen_width 16

// set pin numbers:
const byte stateLED =  16;      // State LED hooked onto Port 3 AIO (PC2)
const int buzzPin = 6;          // State LED hooked into Port 3 DIO (PD6)

const int ResetPinRelayPin = 7;      // FIXME Just a guess, either 7 or 17
const int IgnitionStatePin = 4; // FIXME See above...

byte BuzzLowTone = 196;         // Buzzer low tone
byte BuzzHighTone = 240;	      // Buzzer high tone

boolean flasher = 0;            // LED state level
boolean IgnitionState = 0;
boolean OldIgnitionState = 0;
boolean RaspberryPi = 0;

int PowerFail = 0;
int PowerOK = 1;
int PiAlive = 2;
int PiShuttingDown = 3;

//String MessageFromPi;
//String MessageToPi;
int NumberFromPi;
int NumberToPi;

void BeepAlert(int tonevalue) {
	tone(buzzPin,tonevalue,250);
	delay(250);
	noTone(buzzPin);
	tone(buzzPin,tonevalue,250);
	delay(250);
	noTone(buzzPin);
}

// callback for received data
void receiveData(int byteCount){
	if (DEBUG) {
		Serial.print("I2C data received : ");
	}
	while(Wire.available()) {
  	//MessageFromPi = Wire.read();
		NumberFromPi = Wire.read();
		Serial.println(String(NumberFromPi));
	}
}

// callback for sending data
void sendData(){
	// http://arduino.stackexchange.com/questions/9071/how-do-i-send-a-string-to-master-using-i2c
	//Wire.write(MessageToPi.c_str());
	Serial.print("Sending : ");
	Serial.println(String(NumberToPi));
	Wire.write(NumberToPi);
}

void setup() {
	Serial.begin(57600);    // ...set up the serial ouput on 0004 style
	Serial.println("\n[jeepower]");

	// Initialize the RF12 module. Node ID 30, 868MHZ, Network group 5
	// rf12_initialize(30, RF12_868MHZ, 5);

	// This calls rf12_initialize() with settings obtained from EEPROM address 0x20 .. 0x3F.
	// These settings can be filled in by the RF12demo sketch in the RF12 library
	rf12_config();

  // Set up the easy transmission mechanism
	rf12_easyInit(0);

	// Set up the LCD display
	lcd.begin(screen_width, screen_height);
	lcd.print("[jeepower]");

  pinMode(ResetPinRelayPin, OUTPUT);
  digitalWrite(ResetPinRelayPin, LOW);

	// initialize i2c as slave
	Wire.begin(SLAVE_ADDRESS);

  // define callbacks for i2c communication
  Wire.onReceive(receiveData);
	Wire.onRequest(sendData);

  pinMode(IgnitionStatePin, INPUT_PULLUP);

	// initialize the LED pins as outputs:
	pinMode(stateLED, OUTPUT);

	for (byte i = 0; i <= 10; ++i) {
		digitalWrite(stateLED, flasher);
		if (flasher) {tone(buzzPin,BuzzHighTone,1000); }
		delay(250);
		if (flasher) {noTone(buzzPin); }
	    flasher = !flasher;
	}
	if (DEBUG) { Serial.println("Ready"); }
	if (DEBUG) {
		int val = 100;
		rf12_easyPoll();
		rf12_easySend(&val, sizeof val);
		rf12_easyPoll();
	}
}

void loop(){

	// Sleepy::loseSomeTime() screws up serial output
	//if (!DEBUG) {Sleepy::loseSomeTime(30000);}      // Snooze for 30 seconds
	unsigned long CurrentMillis = millis();

	if (rf12_recvDone() && rf12_crc == 0 && rf12_len == 1) {
		if (DEBUG) { Serial.print("RF12 Received : "); Serial.println(rf12_data[0]); }
	}

    IgnitionState = !digitalRead(IgnitionStatePin);

	if (DEBUG) {
		// lcd.clear();
		lcd.setCursor(0,0);
		lcd.print("CurrentMillis : ");
		lcd.print(String(CurrentMillis));
		lcd.setCursor(0,3);
		lcd.print("CPU:");
		lcd.print(String(RaspberryPi));
		lcd.setCursor(5,3);
		lcd.print("Ign:");
		lcd.print(String(IgnitionState));
	}


	if ( IgnitionState == 1 && RaspberryPi == 1 ) {
		// Everything is fine
		//MessageToPi = 'POWEROK';
		NumberToPi = PowerOK;
	}

	if ( IgnitionState == 1 && RaspberryPi == 0 ) {
		// Power is on, but no RaspberryPi detected
		//MessageToPi = 'POWEROK';
		NumberToPi = PowerOK;
	}

	if ( IgnitionState == 0 && RaspberryPi == 1 ) {
		// Power is off, but RaspberryPi is still running
		//MessageToPi = 'POWERFAIL';
		NumberToPi = PowerFail;
	}

	if (IgnitionState == 0 && RaspberryPi == 0 ) {
		// Power is off, and RaspberryPi is off
	}

}
