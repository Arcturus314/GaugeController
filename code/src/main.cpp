/*
 * main.c
 * Kaveh Pezeshki 6/9/2021
 * Simple vacuum gauge controller using the SSR controller PCB
 */

#include "Arduino.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "gaugepinout.h"
#include "Adafruit_LEDBackpack.h"
#include "math.h"

#define INTERLOCK_THRESHOLD_PRESSURE 1e-5 // interlock LOW below this threshold (mbar)
#define INTERLOCK_THRESHOLD_PRESSURE_HYST 1.2e-5 // interlock HIGH above this threshold (mbar)
#define GAUGE_ID_RESISTANCE 85e3
#define ALLOWED_ID_VARIANCE 100 // ADC increments, so steps of 5mV


Adafruit_SSD1306 display(128, 64, &Wire);
Adafruit_7segment matrix = Adafruit_7segment();

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
    float trueVoltage = involtage/1023.0 * 21.47 * 1.014; // 10 bit ADC so only 1000 input steps
    Serial.println("voltage");
    Serial.println(trueVoltage);

    float unitCoeff = 0;

    switch(units) {
        case 0: // mbar
            unitCoeff = 11.33;
            break;
        case 1: // torr
            unitCoeff = 11.46;
            break;
        case 2: // Pa
            unitCoeff = 9.333;
            break;
    }

    float pressure = pow(10, 1.667*trueVoltage - unitCoeff);
    Serial.println("pressure");
    Serial.println(pressure);
    return pressure;
}

bool checkGaugeID() {
    // returns true if gauge identification correct, 0 otherwise

    int expectedVoltage = (int) ( (float) GAUGE_ID_RESISTANCE / ( (float) GAUGE_ID_RESISTANCE + 1e5) * 1024 );
    int measuredVoltage = analogRead(gauge.id);
    Serial.println("check ID voltage");
    Serial.println(measuredVoltage);

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

    digitalWrite(gauge.vccen, LOW);
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
    digitalWrite(gauge.vccen, HIGH);
    poweren = false;
    digitalWrite(ui.gaugePowerLED, LOW);
}

void gaugePowerOnUI() {
    // gauge power on through the user interface
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println(F("Press SEL to\ninitialize\ngauge"));
    display.setTextSize(1);
    matrix.print(10000, DEC);
    matrix.writeDisplay();
    display.display();

    while (digitalRead(ui.selSw));
    delay(100);

    bool powerOnStatus = gaugePowerOn();

    display.setTextSize(2);

    while (!powerOnStatus) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println(F("Gauge init\nfailed"));
        display.println(F("Press SEL to retry"));
        while (digitalRead(ui.selSw));
        powerOnStatus = gaugePowerOn();
        matrix.print(10000, DEC);
        matrix.writeDisplay();
        display.display();
    }
    delay(100);

    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("Gauge\ninit\ncomplete"));
    matrix.print(10000, DEC);
    matrix.writeDisplay();
    display.display();
    display.setTextSize(1);
    delay(1000);
}


int readGauge() {
    // returns gauge output, reads status and updates global variables
    // checks identification, turns off gauge if identification incorrect

    // checking identification
    if (!checkGaugeID()) {
        gaugePowerOff();
        digitalWrite(output.relay, HIGH);
        digitalWrite(ui.interlockLED, LOW);
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(2);
        display.println(F("GAUGE ERR\n"));
        display.println(F("Press SEL to reset"));
        display.setTextSize(1);
        matrix.print(10000, DEC);
        matrix.writeDisplay();
        display.display();
        while (digitalRead(ui.selSw));
        gaugePowerOnUI();
    }

    // checking high voltage
    if (analogRead(gauge.status) > 400) hvstatus = true;
    else hvstatus = false;

    // sampling and returning
    return analogRead(gauge.signal);
}

void displayInfo() {
    // display enabled channels, expected sensor, and interlock threshold voltage
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,0);
    //display.println(F("CCPG-L2-6"));
    display.print(F("ILK(mbar):"));
    char threshStr[8];
    dtostre(INTERLOCK_THRESHOLD_PRESSURE, threshStr, 1, 0);
    display.println(threshStr);
    display.print(F("PWR: "));
    display.println(poweren);
    display.print(F("HV: "));
    display.println(hvstatus);
    matrix.print(10000, DEC);
    matrix.writeDisplay();
    display.display();
    while (!digitalRead(ui.nxtSw));
    delay(2000);
}

int invertDisplayCounter = 0;
bool invertDisplayVal = false;

void setup() {
    Serial.begin(9600);
    Serial.println(F("MBE Vacuum Gauge Controller V0.1"));

    Serial.println(F("Initializing IO"));

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
    digitalWrite(gauge.vccen, HIGH); // this is active LOW
    digitalWrite(gauge.hven, LOW);
    digitalWrite(gauge.nhven, HIGH);
    digitalWrite(ui.gaugePowerLED, LOW);
    digitalWrite(output.relay, HIGH);


    while(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("display init failed"));
        delay(500);
    }

    matrix.begin(0x70);
    matrix.setBrightness(2);
    matrix.print(10000, DEC);
    matrix.writeDisplay();

    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("FALSON LAB"));
    display.println(F("CCPG-L2-6 INTERFACE"));
    display.display();
    display.setTextSize(1);

    Serial.println(F("Init complete"));


    delay(5000);

    gaugePowerOnUI();



}


void loop() {

    float signalVoltage = readGauge();

    float pressure_units = transform(signalVoltage, selUnits); // in user-selected units
    float pressure_mbar = transform(signalVoltage, 0);

    int   pressure_units_exponent = (int)log10(fabs(pressure_units))-1;
    float pressure_units_mantissa = pressure_units / pow(10, pressure_units_exponent);


    matrix.printFloat(pressure_units_mantissa, 3, 10);

    if (pressure_units_exponent > -1) matrix.writeDigitRaw(3, 0x73);
    else matrix.writeDigitRaw(3, 0x40);

    switch (pressure_units_exponent) {
        case -9: matrix.writeDigitRaw(4,0x6F); break;
        case -8: matrix.writeDigitRaw(4,0x7F); break;
        case -7: matrix.writeDigitRaw(4,0x07); break;
        case -6: matrix.writeDigitRaw(4,0x7D); break;
        case -5: matrix.writeDigitRaw(4,0x6D); break;
        case -4: matrix.writeDigitRaw(4,0x66); break;
        case -3: matrix.writeDigitRaw(4,0x4F); break;
        case -2: matrix.writeDigitRaw(4,0x5B); break;
        case -1: matrix.writeDigitRaw(4,0x06); break;
        case 0:  matrix.writeDigitRaw(4,0x3F); break;
        case 1:  matrix.writeDigitRaw(4,0x06); break;
        case 2:  matrix.writeDigitRaw(4,0x5B); break;
        case 3:  matrix.writeDigitRaw(4,0x4F); break;
        default: matrix.writeDigitRaw(4,0x79); break;
    }


    matrix.writeDisplay();


    display.clearDisplay();
    display.setCursor(1,1);
    display.setTextSize(4);
    display.print("E");
    display.setCursor(40, 1);
    display.print(pressure_units_exponent);
    display.setCursor(60,40);

    // Serial.println(pressureUnitsStr);

    display.setTextSize(2);

    switch(selUnits) {
        case 0: // mbar
            display.println(F("mbar"));
            break;
        case 1: // torr
            display.println(F("torr"));
            break;
        case 2: // Pa
            display.println(F("Pa"));
            break;
    }
    display.display();
    display.setTextSize(1);

    if (pressure_mbar > INTERLOCK_THRESHOLD_PRESSURE_HYST && poweren == true) { // Exceeded threshold
        digitalWrite(output.relay, HIGH); // turning off interlock
        digitalWrite(ui.interlockLED, LOW); // front panel notification LED
    }

    else if (pressure_mbar < INTERLOCK_THRESHOLD_PRESSURE) { // normal safe conditions
        digitalWrite(output.relay, LOW); // turning on interlock
        digitalWrite(ui.interlockLED, HIGH); // front panel notification LED
    }

    // other UI code

    // unit selection
    if (!digitalRead(ui.selSw)) {
        selUnits += 1;
        if (selUnits == 3) selUnits = 0;
    }

    // info screen
    if (!digitalRead(ui.nxtSw)) displayInfo();

    // inverting OLED to prevent burn-in
    delay(300);

    invertDisplayCounter += 1;

    if (invertDisplayCounter % 100 == 0) {
        invertDisplayVal = !invertDisplayVal;
        display.invertDisplay(invertDisplayVal);
    }

}
