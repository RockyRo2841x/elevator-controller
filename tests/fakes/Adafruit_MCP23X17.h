#pragma once
#include "Arduino.h"
inline uint8_t fakePortA = 0xFF, fakePortB = 0xFF;
struct Adafruit_MCP23X17 {
  bool begin_I2C(int) { return true; }
  void pinMode(int, int) {}
  uint8_t readGPIOA() { return fakePortA; }
  uint8_t readGPIOB() { return fakePortB; }
};
