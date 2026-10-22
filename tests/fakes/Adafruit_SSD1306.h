#pragma once
#include "Wire.h"
constexpr int SSD1306_SWITCHCAPVCC = 2, WHITE = 1;
struct Adafruit_SSD1306 {
  Adafruit_SSD1306(int, int, FakeWire*, int) {}
  bool begin(int, int) { return true; }
  void clearDisplay() {}
  void setTextSize(int) {}
  void setTextColor(int) {}
  void setCursor(int, int) {}
  template<class T> void print(const T&) {}
  void display() {}
};
