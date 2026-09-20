



#pragma once

#define HAPTIC_ENABLE_PIN GP18
// Change this if you are using a different pin for the solenoid:
#define SOLENOID_PIN GP19
// If you are not using a solenoid then comment out the above, and also in rules.mk, remove HAPTIC_ENABLE
// We disable haptic feedbeck during USB low power conditions:
#define HAPTIC_OFF_IN_LOW_POWER 1
// You can also tune the following for your solenoid:
#define SOLENOID_DEFAULT_DWELL 5
#define SOLENOID_MIN_DWELL 4
//#define SOLENOID_MAX_DWELL 100
#define NO_HAPTIC_MOD
