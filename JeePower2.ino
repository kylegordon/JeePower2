// vim :ts=3:sw=4:sts=4
/*
	 Control relay actived PSU by monitoring ignition and oil pressure state.

	 created 2011
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

RF12 RF12;

Port optoIn (1);     // Port 1 : Optoisolator inputs
PortI2C myI2C (2);      // Port 2 : I2C driven LCD display for debugging
// Port 3 : Buzzer on DIO and LED on AIO
Port relays (4);     // Port 4 : Output relays

LiquidCrystalI2C lcd (myI2C);

// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

boolean DEBUG = 1;

// set pin numbers:
const byte stateLED =  16;      // State LED hooked onto Port 3 AIO (PC2)
const int buzzPin = 6;          // State LED hooked into Port 3 DIO (PD6)

int IgnitionOnTimeout =	30000;	  // Timeout before confirming ignition is on
int IgnitionOffTimeout = 30000;  // Timeout before confirming ignition is off (and turning off the GPIO relay)
int OilPressureOffTimeout = 1000; // Timeout before confirming oil pressure warning is off
int IgnitionOnMillis = 0;	// Time the ignition came on
int IgnitionOffMillis = 0;	// Time the ignition went off
int OilPressureOffMillis = 0;	// time the oil pressure warning went off

int GPIOOffTime = 0;				  // Time the GPIO relay is disabled

int mainontimeout = 30000;      // time to wait before turning on (30 seconds)
int mainofftimeout = 90000;      // time to wait before turning off (15 minutes)
int GPIOOffTimeout = 10000;     // time to give the Bifferboard to shut down (10 minutes)

long previousMillis = 0;        // last update time
long elapsedMillis = 0;         // elapsed time
long storedMillis = 0;

long flashtarget = 0;           // Used for flashing the LED to indicate what is happening
byte buzzTone = 196;            // Buzzer tone

boolean MainPowerState = 0;	  // Main ATX Relay state
boolean GPIOState = 0;			  // GPIO Relay state

boolean active = 0;
boolean timestored = 0;
boolean flasher = 0;            // LED state level
//boolean ignitionState = 0;      // variable for reading the pushbutton status
//boolean oilState = 0;           // variable for oil pressure state


void setup() {
	 if (DEBUG) {                // If we want to see the pin values for debugging...
		  Serial.begin(57600);    // ...set up the serial ouput on 0004 style
		  Serial.println("\n[cartracker]");
	 }

	 // Initialize the RF12 module. Node ID 30, 868MHZ, Network group 5
	 // rf12_initialize(30, RF12_868MHZ, 5);

	 // This calls rf12_initialize() with settings obtained from EEPROM address 0x20 .. 0x3F.
	 // These settings can be filled in by the RF12demo sketch in the RF12 library
	 rf12_config();

	 // Set up the easy transmission mechanism
	 rf12_easyInit(0);

	 // Set up the relays as digital output devices
	 relays.digiWrite(0);
	 relays.mode(OUTPUT);
	 relays.digiWrite2(0);
	 relays.mode2(OUTPUT);

	 // connect to opto-coupler plug as inputs with pull-ups enabled
	 optoIn.digiWrite(1);
	 optoIn.mode(INPUT);
	 optoIn.digiWrite2(1);
	 optoIn.mode2(INPUT);

	 // initialize the LED pins as outputs:
	 pinMode(stateLED, OUTPUT);

	 for (byte i = 0; i <= 10; ++i) {
		  digitalWrite(stateLED, flasher);
		  if (flasher) {tone(buzzPin,buzzTone,1000); }
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

// Take the time for every variable at the start
// Compare each variable on every loop
// If a variable reaches its limit, do something
// Have a variable used to indicate whether time should be stored?


void loop(){

	 // Sleepy::loseSomeTime() screws up serial output
	 //if (!DEBUG) {Sleepy::loseSomeTime(30000);}      // Snooze for 30 seconds
	 unsigned long currentMillis = millis();

	 if (rf12_recvDone() && rf12_crc == 0 && rf12_len == 1) {
		  if (DEBUG == 1) { Serial.print("Recieved : "); Serial.println(rf12_data[0]); }
	 }

	 // read the state of the ignition and oil pressure to tell if engine is running
	 boolean ignitionState = !optoIn.digiRead2();
	 boolean oilState = !optoIn.digiRead();

	 if (IgnitionOnMillis == 0) {
		  // Ignition has just been turned on, and time has to be stored and made ready for counting up.
		  if (ignitionState == 1 && oilState == 0) {
				// All is compliant. Store the time this happened at
				IgnitionOnMillis = currentMillis;
				if (DEBUG) { Serial.print("Storing ignition turn on time : "); Serial.println(IgnitionOnMillis); }
		  }
	 }

	 if (IgnitionOnMillis != 0 && OilPressureOffMillis != 0) {
		  // Ignition is on
		  IgnitionOffMillis = 0 ;
		  int IgnitionOnElapsedMillis = currentMillis - IgnitionOnMillis;
		  int OilPressureOffElapsedMillis = currentMillis - OilPressureOffMillis;
		  if ((IgnitionOnElapsedMillis > IgnitionOnTimeout) && (OilPressureOffElapsedMillis > OilPressureOffTimeout)) {
				// Turn on the Main output and the GPIO relay
				if (DEBUG) { Serial.print("Turning on"); }
				relays.digiWrite(HIGH); // Turn on the ATX power
				MainPowerState = 1;
				relays.digiWrite2(HIGH); // Turn on the GPIO indicator output
				GPIOState = 1;
		  }
	 }

	 if (ignitionState == 0 && oilState == 0) {
		  // Looks like we're turned off
		  // Wrap this into one if statement?
		  if (IgnitionOffMillis == 0) {
				IgnitionOffMillis = currentMillis;
		  }

	 }
	 if (IgnitionOffMillis != 0) {
		  int IgnitionOffElapsedMillis = currentMillis - IgnitionOffMillis;
		  if (IgnitionOffElapsedMillis > IgnitionOffTimeout) {
					 // Looks like the ignition is off and staying off
					 relays.digiWrite2(LOW); // Turn off the GPIO indicator output
					 GPIOState = 0;
					 if (GPIOOffTime == 0) {
						  GPIOOffTime = currentMillis;
					 }
					 }
	 }

	 if ((currentMillis - GPIOOffTime) > GPIOOffTimeout) {
		  // This is now the timeout period. We've turned off the GPIO indicate, so we have to wait for the board to shut down.
	 }		  


	// ==================================
	// Below is scrap

				
	 /*

	 elapsedMillis = currentMillis - storedMillis;
	 if (elapsedMillis > mainontimeout) { active = 1; }


	 if (mainontimeout <= elapsedMillis) {
		  // mainontimeout is less than the elapsed time
		  // Turn everything on
		  if (DEBUG) { Serial.print("mainontimeout expired"); }
	 }
	 if (gpioofftimeout <= elapsedMillis) {
		  // gpioofftimeout is less than the elapsed time
		  // Turn off the GPIO output
		  if (DEBUG) { Serial.print("gpioofftimeout expired"); }
	 }
	 if (mainofftimeout <= elapsedMillis) {
		  // mainofftimeout is less than the elapsed time
		  // Turn the main output off
		  if (DEBUG) { Serial.print("mainofftimeout expired"); }
	 }

	 */
}
