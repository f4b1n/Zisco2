#include <TFT_eSPI.h>
#include <HomeSpan.h>
#include <SoftwareSerial.h>
#include <Mhz19.h>
#include <SerialCom.h>
#include "Types.hpp"
#include <Smoothed.h>
#include "Roboto20.h"
#include "Roboto70.h"
#include "Roboto40.h"


#define MHZ19_TX_PIN		 17 //update pin
#define MHZ19_RX_PIN		 13 //update pin
#define INTERVAL			 10	  // in seconds
#define HOMEKIT_CO2_TRIGGER	 1350 // co2 level, at which HomeKit alarm will be triggered
#define SMOOTHING_COEFF		 10  // Number of elements in the vector of previous values

bool				  needToWarmUp	= true;
int					  tick			= 0;

particleSensorState_t state;
Smoothed<float>		  mySensor_co2;

// Declare functions

////////////////////////////////////
//   DEVICE-SPECIFIC LED SERVICES //
////////////////////////////////////

// Use software serial
SoftwareSerial mhzSerial(MHZ19_TX_PIN, MHZ19_RX_PIN);
Mhz19 sensor;

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library


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

    sensor.begin(&mhzSerial);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

		// Enable auto-calibration
    sensor.setMeasuringRange(Mhz19MeasuringRange::Ppm_5000);
    sensor.enableAutoBaseCalibration();

		mySensor_co2.begin(SMOOTHED_EXPONENTIAL, SMOOTHING_COEFF); // SMOOTHED_AVERAGE, SMOOTHED_EXPONENTIAL options
	}

	void loop() {

    int h = tft.height();
    int w = tft.width();   
    tft.loadFont(Roboto40);


		if (co2StatusActive->timeVal() > 5 * 1000 && needToWarmUp) {
			// Serial.println("Need to warm up");

			if (!sensor.isReady()) {
				Serial.println("Warming up");
				tft.setTextColor(TFT_RED, TFT_BLACK);
				tft.drawString("warming up ", 20, 20);
        tft.drawString(String(tick) + " s", w / 3, 70);
				delay(2.5 * 1000);
        tft.fillScreen(TFT_BLACK);
				tick = tick + 5;
				Serial.println((String)tick + " ");
				co2StatusActive->setVal(false);
			} else {
				needToWarmUp = false;
				co2StatusActive->setVal(true);
				Serial.println("Is warmed up");
				tft.setTextColor(TFT_GREEN);
				tft.drawString("warmed up", 10, h / 2 - 32);
			}
		}

		if (co2Level->timeVal() > INTERVAL * 1000 && !needToWarmUp) { // check time elapsed since last update and proceed only if greater than 5 seconds

      auto co2_value = sensor.getCarbonDioxide();

			if (sensor.isReady()) {
				auto co2_value = sensor.getCarbonDioxide();
			}

			if (co2_value >= 400) {

				// Read a sensor value
				// print co2 value
				LOG1("CO2: ");
				LOG1(co2_value);
				LOG1(" ppm\n");

        tft.fillScreen(TFT_BLACK);
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
          tft.unloadFont();
          tft.loadFont(Roboto70);
					tft.setTextColor(TFT_RED, TFT_BLACK); // red color
					String co2_string = String(co2_value);;
          tft.drawString(co2_string, 20, h / 2 - 32);
          tft.unloadFont();
          tft.loadFont(Roboto20);          
          tft.drawString("co2", 190, h / 3 + 5);
          tft.drawString("ppm", 190, h / 2 + 5);


				} else if (co2_value >= 800) {
					LOG1("Yellow\n");
					String co2_string = String(co2_value) + " co2";
          tft.drawString(co2_string, 20, tft.height() / 2 - 32);
				} else if (co2_value >= 400) {
					LOG1("Green color\n");
					String co2_string = String(co2_value) + " co2";
          tft.drawString(co2_string, 20, tft.height() / 2 - 32);
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

