// vim :ts=3:sw=4:sts=4
/*
	Control relay activated PSU by monitoring ignition and oil pressure state.
	A timer controls the power up state to ensure the engine is running and stable before bringing up the system
	Another timer will trigger when ignition is lost. If ignition is off for over a certain time, another relay is
	turned off to signal the Bifferboard. Once a secondary timer expires, the main power relay is deactivated.

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

Port optoIn (1);     // Port 1 : Optoisolator inputs
PortI2C myI2C (2);      // Port 2 : I2C driven LCD display for debugging
// Port 3 : Buzzer on DIO and LED on AIO
Port relays (4);     // Port 4 : Output relays

// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

boolean DEBUG = 0;

// set pin numbers:
const byte stateLED =  16;      // State LED hooked onto Port 3 AIO (PC2)
const int buzzPin = 6;          // State LED hooked into Port 3 DIO (PD6)

int IgnitionOnTimeout =	30000;		// Timeout before confirming ignition is on
int IgnitionOffTimeout = 30000;		// Timeout before confirming ignition is off (and turning off the GPIO relay)
int OilPressureOffTimeout = 30000;	// Timeout before confirming oil pressure warning is off
long IgnitionOnMillis = 0;				// Time the ignition came on
long IgnitionOffMillis = 0;			// Time the ignition went off
long OilPressureOffMillis = 0;		// time the oil pressure warning went off
int GPIOOffTimeout = 300000;		// This is the time between turning off the GPIO relay, and turning off the main relay (10 minutes)

long GPIOOffTime = 0;					// Time the GPIO relay is disabled

byte BuzzLowTone = 196;            // Buzzer low tone
byte BuzzHighTone = 240;	// Buzzer high tone

boolean MainPowerState = 0;	  // Main ATX Relay state
boolean GPIOState = 0;			  // GPIO Relay state

boolean flasher = 0;            // LED state level
boolean OldIgnitionState = 0;   // Used to compare the ignition state
boolean OldOilState = 0;        // Used to compare the oil warning state

void setup() {
	Serial.begin(57600);    // ...set up the serial ouput on 0004 style
	Serial.println("\n[cartracker]");

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
		  if (flasher) {tone(buzzPin,BuzzLowTone,1000); }
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

void loop(){

	 // Sleepy::loseSomeTime() screws up serial output
	 //if (!DEBUG) {Sleepy::loseSomeTime(30000);}      // Snooze for 30 seconds
	 unsigned long CurrentMillis = millis();

	 if (rf12_recvDone() && rf12_crc == 0 && rf12_len == 1) {
		  if (DEBUG) { Serial.print("Recieved : "); Serial.println(rf12_data[0]); }
	 }

	 // read the state of the ignition and oil pressure to tell if engine is running
	 boolean IgnitionState = !optoIn.digiRead2();
	 boolean OilState = !optoIn.digiRead();

	 // If anything has changed, beep for a moment and take a slight pause
	 if (OldOilState != OilState || OldIgnitionState != IgnitionState) {
		  tone(buzzPin,BuzzHighTone,250);
		  delay(250);
		  noTone(buzzPin);
		  tone(buzzPin,BuzzHighTone,250);
		  delay(250);
		  noTone(buzzPin);
		  delay(1000);
	 }
	 OldIgnitionState = IgnitionState;
	 OldOilState = OilState;


	 if (OilState  == 1 && OilPressureOffMillis != 0) {
		  // The oil light is on. We're not ready to start up yet
		  if (DEBUG) { Serial.println("Oil pressure warning."); }
		  delay(1000);
		  OilPressureOffMillis = 0;
	 }
	 if (OilState == 0 && OilPressureOffMillis == 0) {
		  // The oil light is off. We're good to go
		  if (DEBUG) { Serial.println("Oil pressure ok."); }
		  OilPressureOffMillis = CurrentMillis;
		  delay(1000);
	 }

	 if (IgnitionState == 1 && IgnitionOnMillis == 0) {
		  // Ignition has just been turned on, and time has to be stored and made ready for counting up.
		  // All is compliant. Store the time this happened at
		  IgnitionOnMillis = CurrentMillis;
		  if (DEBUG) { Serial.print("Storing ignition turn on time : "); Serial.println(IgnitionOnMillis); }
		  delay(1000);
	 }

	 if (IgnitionOnMillis != 0 && OilPressureOffMillis != 0) {
		  // Ignition is on
		  IgnitionOffMillis = 0 ;
		  long IgnitionOnElapsedMillis = CurrentMillis - IgnitionOnMillis;
		  long OilPressureOffElapsedMillis = CurrentMillis - OilPressureOffMillis;
		  Serial.print("IgnitionOnElapsedMillis     : "); Serial.println(IgnitionOnElapsedMillis);
		  Serial.print("OilPressureOffElapsedMillis : "); Serial.println(OilPressureOffElapsedMillis);
		  if ((IgnitionOnElapsedMillis < IgnitionOnTimeout) && (MainPowerState == 0)) { 
				//&& (OilPressureOffElapsedMillis < OilPressureOffTimeout)) {
				// Only whilst counting upwards, buzz periodically to indicate that we're in this state.
				// Maybe a low, high tone?
				digitalWrite(stateLED, flasher);
				if (flasher) {
					 tone(buzzPin,BuzzLowTone,250);
					 delay(250);
					 tone(buzzPin,BuzzHighTone,250);
					 delay(250);
					 noTone(buzzPin); 
				}
				// flasher = !flasher;
				delay(250);
		  }
		  if ((IgnitionOnElapsedMillis > IgnitionOnTimeout) && (OilPressureOffElapsedMillis > OilPressureOffTimeout)) {
				// Turn on the Main output and the GPIO relay
				// This should be the main function when in a running state
				if (DEBUG) { Serial.println("Main power and GPIO is on"); }
				relays.digiWrite(HIGH); // Turn on the ATX power
				MainPowerState = 1;
				relays.digiWrite2(HIGH); // Turn on the GPIO indicator output
				GPIOState = 1;
				GPIOOffTime = 0; // Could this be combined with GPIOState?
				digitalWrite(stateLED, HIGH);
		  }
	 }

	 if (IgnitionState == 0 && OilState == 0) {
		  // Looks like we're turned off
		  // Wrap this into one if statement?
		  if (IgnitionOffMillis == 0) {
				IgnitionOffMillis = CurrentMillis;
				IgnitionOnMillis = 0;
		  }

	 }

	 if (IgnitionOffMillis != 0) {
		  long IgnitionOffElapsedMillis = CurrentMillis - IgnitionOffMillis;
		  Serial.print("IgnitionOffElapsedMillis     : "); Serial.println(IgnitionOffElapsedMillis);
		  digitalWrite(stateLED, flasher);
		  // flasher = !flasher;
		  if (IgnitionOffElapsedMillis > IgnitionOffTimeout) {
				// Looks like the ignition is off and staying off
				relays.digiWrite2(LOW); // Turn off the GPIO indicator output
				GPIOState = 0;
				if (GPIOOffTime == 0) {
					 GPIOOffTime = CurrentMillis;
				}
		  }
	 }

	 // If Elapsed GPIO Off time is less than the timeout, indicate that we're counting down
	 if (((CurrentMillis - GPIOOffTime) < GPIOOffTimeout) && (MainPowerState == 1)) {
		  // We're waiting for the main timeout to expire now
		  if (DEBUG) { Serial.println("GPIO off, main power still on. Waiting."); }
		  // delay(1000);
		  // Have a periodic buzzer tone to indicate this state
		  // maybe a high, low tone?
		  digitalWrite(stateLED, flasher);
		  if (flasher) {
				tone(buzzPin,BuzzHighTone,250);
				delay(250);
				tone(buzzPin,BuzzLowTone,250);
				delay(250);
				noTone(buzzPin);       
		  }
		  // flasher = !flasher;
		  delay(250);

	 }

	 if (GPIOOffTime != 0 && (CurrentMillis - GPIOOffTime) > GPIOOffTimeout) {
		  // The GPIO timeout has expired. Turn off the main output
		  if (DEBUG) { Serial.println("Everything is off"); }
		  // Indicate via a signature buzz tone
		  relays.digiWrite(LOW); // Turn off the ATX power
		  MainPowerState = 0;
		  IgnitionOffMillis = 0;
	 }

	 flasher = !flasher;

	 if (DEBUG) { 
		  Serial.println("---");
		  Serial.print("CurrentMillis               : "); Serial.println(CurrentMillis);
		  Serial.print("IgnitionOnTimeout           : "); Serial.println(IgnitionOnTimeout);
		  Serial.print("IgnitionOffTimeout          : "); Serial.println(IgnitionOffTimeout); 
		  Serial.print("OilPressureOffTimeout       : "); Serial.println(OilPressureOffTimeout);
		  Serial.print("IgnitionOnMillis            : "); Serial.println(IgnitionOnMillis);
		  Serial.print("IgnitionOffMillis           : "); Serial.println(IgnitionOffMillis);
		  Serial.print("OilPressureOffMillis        : "); Serial.println(OilPressureOffMillis);
		  Serial.print("GPIOOffTimeout              : "); Serial.println(GPIOOffTimeout);
		  Serial.print("GPIOOffTime                 : "); Serial.println(GPIOOffTime);
		  Serial.print("IgnitionState               : "); Serial.println(IgnitionState);
		  Serial.print("OilState                    : "); Serial.println(OilState);
		  Serial.print("MainPowerState              : "); Serial.println(MainPowerState);
		  Serial.println("===");
		  delay(1000);
	 }

}
