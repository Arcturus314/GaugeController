/*
 * gaugepinout.h
 * Kaveh Pezeshki 6/10/2021
 * mapping from the SSR controller board pinout to the gauge controller logical names
 */

#ifndef gaugepinout_h
#define gaugepinout_h

#include "Arduino.h"
#include "boardpinout.h"

struct ui_struct {
    const int selSw = generalio.selSw;
    const int nxtSw = generalio.nxtSw;
    const int interlockLED = ch0.outPin;
    const int gaugePowerLED = ch1.outPin;
} ui;

struct gauge_struct {
    const int vccen = ch1.outDriver; // supply + line provided via relay
    const int hven = ch2.outDriver;  // + high voltage enable. Need >11V
    const int nhven = ch2.outPin;    // - high voltage enable. Need <2.5V
    const int status = ch1.ainB;     // up to supply voltage
    const int id = ch2.ainB;         // pull up to 5V on our end. Resistor 100kOhm
    const int signal = ch0.ainB;     // 0-10.5V
} gauge;

struct output_struct {
    const int relay = ch0.outDriver; // interlock output relay
} output;

#endif
