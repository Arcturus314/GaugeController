/*
 * main.c
 * Kaveh Pezeshki 6/9/2021
 * Simple vacuum gauge controller using the SSR controller PCB
 */

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "gaugepinout.h"
#include "math.h"

#define INTERLOCK_THRESHOLD_PRESSURE 1e-6 // interlock HIGH above this threshold (mbar)
#define GAUGE_ID_RESISTANCE 1e5
#define ALLOWED_ID_VARIANCE 10 // ADC increments, so steps of 5mV

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

bool poweren = false;
bool hvstatus = false;
int selUnits = 0;

float transform(float involtage, int units) {
    // Transforms voltage output from vacuum sensor to real-world pressure
    // Expects input voltage 0-10V with 3:1 voltage divider. So full-scale would be 3.33V
    // Supports several units, selected by parameter 'units'
    // units:
    //  0: mbar
    //  1: torr
    //  2: Pa

    // TODO: calibrate to true resistor values
    float trueVoltage = involtage/1023.0 * 16.6667; // 10 bit ADC so only 1000 input steps

    float unitCoeff = 0;

    switch(units) {
        case 0: // mbar
            unitCoeff = 12.66;
            break;
        case 1: // torr
            unitCoeff = 12.826;
            break;
        case 2: // Pa
            unitCoeff = 10;
            break;
    }

    float pressure = pow(10, 0.75*(trueVoltage - unitCoeff));
    return pressure;
}

void setup() {
    Serial.begin(9600);
    Serial.println("MBE Vacuum Gauge Controller V0.1");

    Serial.println("Initializing IO");

    pinMode(ui.selSw, INPUT);
    pinMode(ui.nxtSw, INPUT);
    pinMode(ui.interlockLED, OUTPUT);

    pinMode(gauge.vccen, OUTPUT);
    pinMode(gauge.hven, OUTPUT);
    pinMode(gauge.nhven, OUTPUT);
    // gauge.status: analog input
    // gauge.idAn: analog input
    // gauge.signal: analog input

    pinMode(output.relay, OUTPUT);

    // basic initialization
    digitalWrite(gauge.vccen, LOW);
    digitalWrite(gauge.hven, LOW);
    digitalWrite(gauge.nhven, HIGH);
    digitalWrite(ui.gaugePowerLED, LOW);

    display.display();
    delay(100);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("FALSON GROUP");
    display.println("CCPG-L2-6 INTERFACE");
    display.display();

    delay(1000);

}

bool checkGaugeID() {
    // returns true if gauge identification correct, 0 otherwise

    int expectedVoltage = (int) ( (float) GAUGE_ID_RESISTANCE / ( (float) GAUGE_ID_RESISTANCE + 1e5) * 1024 );
    int measuredVoltage = analogRead(gauge.id);

    if ( abs(expectedVoltage - measuredVoltage) > ALLOWED_ID_VARIANCE ) return false;

    return true;
}

bool gaugePowerOn() {
    // Turns on gauge with identification check
    // 1. Checks identification. If correct, proceeds
    // 2. enables supply power, waits 100ms
    // 3. enables HV power
    // returns true if gauge power-on successful, false if identification check fails

    if (!checkGaugeID()) return false;

    digitalWrite(gauge.vccen, HIGH);
    poweren = true;
    digitalWrite(ui.gaugePowerLED, HIGH);
    delay(100);
    digitalWrite(gauge.hven, HIGH);
    digitalWrite(gauge.nhven, LOW);
    delay(100);

    return true;

}

void gaugePowerOff() {
    // gauge power off, printing user interface message
    // waits for user to power on gauge
    digitalWrite(gauge.hven, LOW);
    digitalWrite(gauge.nhven, LOW);
    delay(100);
    digitalWrite(gauge.vccen, LOW);
    poweren = false;
    digitalWrite(ui.gaugePowerLED, LOW);
}

void gaugePowerOnUI() {
    // gauge power on through the user interface
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Press SEL to");
    display.println("initialize gauge");

    while (digitalRead(ui.selSw));

    bool powerOnStatus = gaugePowerOn();

    while (!powerOnStatus) {
        display.clearDisplay();
        display.println("Gauge init failed");
        display.println("Press SEL to retry");
        while (digitalRead(ui.selSw));
        powerOnStatus = gaugePowerOn();
        display.display();
    }

    display.clearDisplay();
    display.println("Gauge initialized");
    display.display();
    delay(1000);
}


int readGauge() {
    // returns gauge output, reads status and updates global variables
    // checks identification, turns off gauge if identification incorrect

    // checking identification
    if (!checkGaugeID()) {
        gaugePowerOff();
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Gauge disconnected");
        display.println("Press SEL to reset");
        while (digitalRead(ui.selSw));
        gaugePowerOnUI();
    }

    // checking high voltage
    if (digitalRead(gauge.status)) hvstatus = true;
    else hvstatus = false;

    // sampling and returning
    return analogRead(gauge.signal);
}

void displayInfo() {
    // display enabled channels, expected sensor, and interlock threshold voltage
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("CCPG-L2-6");
    display.print("THRESH (mbar): ");
    display.println(INTERLOCK_THRESHOLD_PRESSURE);
    display.print("PWR: ");
    display.println(poweren);
    display.print("HV: ");
    display.println(hvstatus);
    display.display();
    delay(2000);
}

int invertDisplayCounter = 0;
bool invertDisplayVal = false;


void loop() {

    float signalVoltage = readGauge();
    float pressure_units = transform(signalVoltage, selUnits); // in user-selected units
    float pressure_mbar = transform(signalVoltage, 0);

    display.clearDisplay();
    display.setTextSize(5);
    display.println(pressure_units);

    switch(selUnits) {
        case 0: // mbar
            display.println("mbar");
            break;
        case 1: // torr
            display.println("torr");
            break;
        case 2: // Pa
            display.println("Pa");
            break;
    }
    display.display();
    display.setTextSize(1);

    if (pressure_mbar > INTERLOCK_THRESHOLD_PRESSURE) { // Exceeded threshold
        digitalWrite(ch0.outDriver, LOW); // turning off interlock
        digitalWrite(ch0.outPin, LOW); // front panel notification LED
    }

    else { // normal safe conditions
        digitalWrite(ch0.outDriver, HIGH); // turning on interlock
        digitalWrite(ch0.outPin, HIGH); // front panel notification LED
    }

    // other UI code

    // unit selection
    if (!digitalRead(ui.selSw)) {
        selUnits += 1;
        if (selUnits == 4) selUnits = 0;
    }

    // info screen
    if (!digitalRead(ui.nxtSw)) displayInfo();

    // inverting OLED to prevent burn-in
    delay(100);
    invertDisplayCounter += 1;

    if (invertDisplayCounter % 100 == 0) {
        invertDisplayVal = !invertDisplayVal;
        display.invertDisplay(invertDisplayVal);
    }

}
