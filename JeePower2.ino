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
#include <PortsLCD.h>

Port optoIn (1);                // Port 1 : Optoisolator inputs
PortI2C myI2C (2);              // Port 2 : I2C driven LCD display for debugging
// Port 3 : Buzzer on DIO and LED on AIO
Port relays (4);                // Port 4 : Output relays

// has to be defined because we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

boolean DEBUG = 1;

LiquidCrystalI2C lcd (myI2C);
#define screen_height 4
#define screen_width 16

// set pin numbers:
const byte stateLED =  16;      // State LED hooked onto Port 3 AIO (PC2)
const int buzzPin = 6;          // State LED hooked into Port 3 DIO (PD6)

byte BuzzLowTone = 196;         // Buzzer low tone
byte BuzzHighTone = 240;	      // Buzzer high tone

boolean MainPowerState = 0;	    // Main ATX Relay state
boolean flasher = 0;            // LED state level
boolean OldCPUState = 0;
boolean OldIgnitionState = 0;
boolean ShutdownTimer = 0;			// shutdown timer counter. FIXME Use proper timers
boolean StartupTimer = 0; 			// Startup timer to let the system boot before being able to send a shutdown signal

void BeepAlert(int tonevalue) {
	tone(buzzPin,tonevalue,250);
	delay(250);
	noTone(buzzPin);
	tone(buzzPin,tonevalue,250);
	delay(250);
	noTone(buzzPin);
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

	 // Set up the relays as digital output devices
	 relays.digiWrite(0);		// ATX power
	 relays.mode(OUTPUT);
	 relays.digiWrite2(0);	// GPIO signal
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
		if (DEBUG) { Serial.print("Recieved : "); Serial.println(rf12_data[0]); }
	}

	// Read the state of the ignition, and the CPU
	boolean IgnitionState = optoIn.digiRead();
	boolean CPUState = optoIn.digiRead2();

	// If anything has changed, beep for a moment and take a slight pause
	/*
	if (OldCPUState != CPUState || OldIgnitionState != IgnitionState) {
		if (DEBUG) {
			Serial.print("IgnitionState    : "); Serial.println(IgnitionState);
			Serial.print("OldIgnitionState : "); Serial.println(OldIgnitionState);
			Serial.print("CPUState         : "); Serial.println(CPUState);
			Serial.print("OldCPUState      : "); Serial.println(OldCPUState);
			delay(5000);
		}
		BeepAlert(BuzzHighTone);
		delay(1000);
	}

	*/

	if (DEBUG) {
		// lcd.clear();
		lcd.setCursor(0,0);
		lcd.print("CurrentMillis : ");
		lcd.print(String(CurrentMillis));
		lcd.setCursor(0,1);
		lcd.print("StartupTimer  : ");
		lcd.print(String(StartupTimer));
		lcd.setCursor(0,2);
		lcd.print("ShutdownTimer : ");
		lcd.print(String(ShutdownTimer));
		lcd.setCursor(0,3);
		lcd.print("CPU:");
		lcd.print(String(CPUState));
		lcd.setCursor(5,3);
		lcd.print("Ign:");
		lcd.print(String(IgnitionState));
	}


/*

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

*/

	if (IgnitionState == 1) {

		ShutdownTimer = 0;

		if (CPUState == 0 && StartupTimer == 0 ) {
			Serial.println("Turning on CPU, ATX and LED");
			StartupTimer = CurrentMillis;
			BeepAlert(BuzzHighTone);
			delay(10000);
			digitalWrite(stateLED, HIGH);
			relays.digiWrite(HIGH);
			relays.digiWrite2(HIGH);
		}
		if (CPUState == 0 && StartupTimer > 60000 ) {
			// Timed out waiting for CPU to boot.
			// Turn it all off and try again.
			BeepAlert(BuzzLowTone);
			digitalWrite(stateLED, LOW);
			relays.digiWrite(LOW);
			relays.digiWrite2(LOW);
			StartupTimer = 0;
		}
		if (CPUState == 1) {
			Serial.println("CPU is reporting as being on");
		}
	}

	if (IgnitionState == 0) {
		if (ShutdownTimer == 0) {
			ShutdownTimer = CurrentMillis;
		} else if (ShutdownTimer > 30000) {
			BeepAlert(BuzzLowTone); // FIXME This will get annoying...
		} else if (ShutdownTimer > 120000) {
			// Turn off ATX, signalling line, and stateLED
			digitalWrite(stateLED, LOW);
			relays.digiWrite(LOW);
			relays.digiWrite2(LOW);
		}
	}

}
