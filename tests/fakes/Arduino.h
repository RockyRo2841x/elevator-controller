#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
using String = std::string;
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT = 0, INPUT_PULLUP = 2, A0 = 14;
inline unsigned long fakeNow = 0;
inline int fakeLoad = 0;
inline std::array<int, 32> fakeDigital{}, fakePWM{};
inline unsigned long millis() { return fakeNow; }
inline int analogRead(int) { return fakeLoad; }
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) { fakeDigital.at(pin) = value; }
inline void analogWrite(int pin, int value) { fakePWM.at(pin) = value; }
inline long map(long x, long lo, long hi, long outlo, long outhi) {
  return (x - lo) * (outhi - outlo) / (hi - lo) + outlo;
}
template<class T> T constrain(T x, T lo, T hi) { return std::clamp(x, lo, hi); }
struct FakeSerial { void begin(int) {} template<class T> void println(const T&) {} };
inline FakeSerial Serial;
#define F(text) text
