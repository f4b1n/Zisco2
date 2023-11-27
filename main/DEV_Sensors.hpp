#include <TFT_eSPI.h>
#include <HomeSpan.h>
#include <SoftwareSerial.h>
#include <ErriezMHZ19B.h> //check this
#include "SerialCom.hpp"
#include "Types.hpp"
#include <Smoothed.h>


// I2C for temp sensor
#include <Wire.h>
#define si7021Addr			 0x40 // I2C address for temp sensor

#define MHZ19B_TX_PIN		 19 //update pin
#define MHZ19B_RX_PIN		 18 //update pin
#define INTERVAL			 10	  // in seconds
#define HOMEKIT_CO2_TRIGGER	 1350 // co2 level, at which HomeKit alarm will be triggered


#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library

bool				  needToWarmUp	= true;
int					  tick			= 0;
Smoothed<float>		  mySensor_co2;

// Declare functions
void   detect_mhz();

////////////////////////////////////
//   DEVICE-SPECIFIC LED SERVICES //
////////////////////////////////////

// Use software serial
SoftwareSerial mhzSerial(MHZ19B_TX_PIN, MHZ19B_RX_PIN);

// Declare MHZ19B object
ErriezMHZ19B mhz19b(&mhzSerial);

struct DEV_CO2Sensor : Service::CarbonDioxideSensor { // A standalone Temperature sensor

	SpanCharacteristic *co2Detected;
	SpanCharacteristic *co2Level;
	SpanCharacteristic *co2PeakLevel;
	SpanCharacteristic *co2StatusActive;

	DEV_CO2Sensor() : Service::CarbonDioxideSensor() { // constructor() method

		co2Detected		= new Characteristic::CarbonDioxideDetected(false);
		co2Level		= new Characteristic::CarbonDioxideLevel(400);
		co2PeakLevel	= new Characteristic::CarbonDioxidePeakLevel(400);
		co2StatusActive = new Characteristic::StatusActive(false);

		Serial.print("Configuring Carbon Dioxide Sensor\n"); // initialization message

		mhzSerial.begin(9600);

		// Enable auto-calibration
		mhz19b.setAutoCalibration(true);

		mySensor_co2.begin(SMOOTHED_EXPONENTIAL, SMOOTHING_COEFF); // SMOOTHED_AVERAGE, SMOOTHED_EXPONENTIAL options
	}

	void loop() {

		if (co2StatusActive->timeVal() > 5 * 1000 && needToWarmUp) {
			// Serial.println("Need to warm up");

			if (mhz19b.isWarmingUp()) {
				Serial.println("Warming up");
				tft.setTextColor(TFT_RED);
				tft.drawString("warming up", tft.width()/2, tft.height()/2-32);
				delay(2.5 * 1000);
				tick = tick + 5;
				Serial.println((String)tick + " ");
				co2StatusActive->setVal(false);
			} else {
				needToWarmUp = false;
				co2StatusActive->setVal(true);
				Serial.println("Is warmed up");
				tft.setTextColor(TFT_GREEN);
				tft.drawString("warmed up", tft.width()/2, tft.height()/2-32);
			}
		}

		if (co2Level->timeVal() > INTERVAL * 1000 && !needToWarmUp) { // check time elapsed since last update and proceed only if greater than 5 seconds

			float co2_value;

			if (mhz19b.isReady()) {
				co2_value = mhz19b.readCO2();
			}

			if (co2_value >= 400) {

				// Read a sensor value
				// print co2 value
				LOG1("CO2: ");
				LOG1(co2_value);
				LOG1(" ppm\n");

				mySensor_co2.add(co2_value);

				co2Level->setVal(mySensor_co2.get()); // set the new co value; this generates an Event Notification and also resets the elapsed time
				LOG1("Carbon Dioxide Update: ");
				LOG1(co2Level->getVal());
				LOG1("\n");

				// Set color indicator
				// 400 - 800    -> green
				// 800 - 1000   -> yellow
				// 1000+        -> red
				if (co2_value >= 1000) {
					LOG1("Red\n");
					tft.setTextColor(TFT_RED); // red color
					String co2_string = String(co2_value) + "co2";
          			tft.drawString(co2_string, tft.width() / 2, tft.height() / 2 - 32);
				} else if (co2_value >= 800) {
					LOG1("Yellow\n");
					String co2_string = String(co2_value) + "co2";
          			tft.drawString(co2_string, tft.width() / 2, tft.height() / 2 - 32);
				} else if (co2_value >= 400) {
					LOG1("Green color\n");
					String co2_string = String(co2_value) + "co2";
          			tft.drawString(co2_string, tft.width() / 2, tft.height() / 2 - 32);
				}

				// Update peak value
				if (co2_value > co2PeakLevel->getVal()) {
					co2PeakLevel->setVal(co2_value);
				}

				// Trigger HomeKit sensor when concentration reaches this level
				if (co2_value > HOMEKIT_CO2_TRIGGER) {
					co2Detected->setVal(true);
				} else {
					co2Detected->setVal(false);
				}
			}
		}

		if (co2Level->timeVal() > 12 * 60 * 60 * 1000) {
			co2PeakLevel->setVal(400);
		}
	}
};

// HELPER FUNCTIONS

void detect_mhz() {
	// Detect sensor
	Serial.println("Detecting MH-Z19B");
	while (!mhz19b.detect()) {
		tft.setTextColor(TFT_RED);
        tft.drawString("Detecting MH-Z19B", tft.width() / 2, tft.height() / 2 - 32);
	};
	Serial.println("Sensor detected!");
}
