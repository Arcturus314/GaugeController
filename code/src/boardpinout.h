/*
 * pinout.h
 * Kaveh Pezeshki 6/9/2021
 * board constants for the SSR controller
 */

#ifndef boardpinout_h
#define boardpinout_h

#include "Arduino.h"

// note: I2C interface not included in this pinout

struct generalio_struct {
    const int selSw = 12;
    const int nxtSw = 12;
} generalio;

struct ch0_struct {
    const int button = 2; // on front panel interface
    const int outDriver = 3; // driven by BJT at board input voltage
    const int outPin = 10; // on front panel interface, driven by Arduino
    const int ainB = A3; // analog in on back panel
    const int ainF = A0; // analog in on front panel
} ch0;

struct ch1_struct {
    const int button = 4;
    const int outDriver = 5;
    const int outPin = 7;
    const int ainB = A6;
    const int ainF = A1;
} ch1;

struct ch2_struct {
    const int button = 8;
    const int outDriver = 9;
    const int outPin = 6;
    const int ainB = A7;
    const int ainF = A2;
} ch2;


#endif
