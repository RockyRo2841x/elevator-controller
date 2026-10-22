#pragma once
#include "Arduino.h"
struct FakeWire { void begin() {} };
inline FakeWire Wire;
