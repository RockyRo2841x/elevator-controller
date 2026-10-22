// Compile the actual prepared sketch against deterministic hardware fakes.
#include <cassert>
#include <climits>
#include <iostream>
#include <random>
#include "../firmware/ElevatorController/ElevatorController.ino"

void resetController(int floor = 1) {
  fakePortA = fakePortB = 0xFF;
  readButtonStates(); // Release any button remembered by the edge detector.
  for(int i = 0; i < 6; ++i) req_internal[i] = req_up[i] = req_down[i] = false;
  currentFloor = floor;
  currentDirection = IDLE;
  systemState = SYS_IDLE;
  stateTimer = fakeNow = 0;
  fakeLoad = currentLoad = basePWM = 0;
  fakeDigital.fill(0);
  fakePWM.fill(0);
}

void tick(unsigned long elapsed) {
  fakeNow += elapsed;
  loop();
  assert(currentFloor >= 1 && currentFloor <= 5);
}

bool pending() {
  for(int i = 1; i <= 5; ++i)
    if(req_internal[i] || req_up[i] || req_down[i]) return true;
  return false;
}

void testInputMapping() {
  for(int floor = 1; floor <= 5; ++floor) {
    resetController();
    fakePortA &= static_cast<uint8_t>(~(1 << (8 - floor)));
    readButtonStates();
    for(int i = 1; i <= 5; ++i) assert(req_internal[i] == (i == floor));
  }
  const int floors[] = {5, 4, 4, 3, 3, 2, 2, 1};
  const bool up[] = {false, true, false, true, false, true, false, true};
  for(int bit = 0; bit < 8; ++bit) {
    resetController();
    fakePortB &= static_cast<uint8_t>(~(1 << bit));
    readButtonStates();
    for(int floor = 1; floor <= 5; ++floor) {
      assert(req_up[floor] == (up[bit] && floor == floors[bit]));
      assert(req_down[floor] == (!up[bit] && floor == floors[bit]));
    }
  }
}

void testArrivalClearingAndHeldButton() {
  resetController();
  fakePortA &= static_cast<uint8_t>(~(1 << 5)); // Internal floor 3.
  tick(0);
  assert(systemState == SYS_TRANSIT && currentDirection == UP);
  tick(1999); assert(currentFloor == 1);
  tick(1); assert(currentFloor == 2 && systemState == SYS_TRANSIT);
  tick(2000); assert(currentFloor == 3 && systemState == SYS_DOORS_OPEN);
  assert(!req_internal[3] && fakePWM[MOTOR_EN] == 0);
  tick(2999); assert(systemState == SYS_DOORS_OPEN);
  tick(1); assert(systemState == SYS_IDLE);
  tick(0); assert(systemState == SYS_IDLE && currentDirection == IDLE && !pending());
  // A held level must not repeatedly open the doors after the request is served.
  tick(6000); assert(systemState == SYS_IDLE && !pending());
  fakePortA = 0xFF; tick(0);
  fakePortA &= static_cast<uint8_t>(~(1 << 5)); tick(0);
  assert(systemState == SYS_DOORS_OPEN);
}

void testOppositeCallAtTopAndBottom() {
  resetController();
  req_down[5] = true;
  tick(0);
  for(int i = 0; i < 4; ++i) tick(2000);
  assert(currentFloor == 5 && systemState == SYS_DOORS_OPEN && !req_down[5]);
  tick(3000); tick(0); tick(10000);
  assert(currentFloor == 5 && systemState == SYS_IDLE && fakePWM[MOTOR_EN] == 0);

  resetController(5);
  req_up[1] = true;
  tick(0);
  for(int i = 0; i < 4; ++i) tick(2000);
  assert(currentFloor == 1 && systemState == SYS_DOORS_OPEN && !req_up[1]);
}

void testDirectionPriorityAndReturnService() {
  resetController();
  req_internal[5] = true;
  req_down[2] = true;
  tick(0); tick(2000);
  assert(currentFloor == 2 && systemState == SYS_TRANSIT && req_down[2]);
  tick(2000); tick(2000); tick(2000);
  assert(currentFloor == 5 && systemState == SYS_DOORS_OPEN && !req_internal[5]);
  tick(3000); tick(0);
  assert(currentDirection == DOWN && systemState == SYS_TRANSIT);
  tick(2000); tick(2000); tick(2000);
  assert(currentFloor == 2 && systemState == SYS_DOORS_OPEN && !pending());
}

void testTurnaroundAtCurrentFloor() {
  resetController(3);
  currentDirection = UP;
  req_down[3] = true;
  req_internal[1] = true;
  tick(0);
  assert(systemState == SYS_DOORS_OPEN && !req_down[3]);
  tick(3000); tick(0);
  assert(currentDirection == DOWN && systemState == SYS_TRANSIT);
}

void testRequestDuringTravel() {
  resetController();
  req_internal[4] = true;
  tick(0);
  fakePortA &= static_cast<uint8_t>(~(1 << 6)); // New internal floor 2.
  tick(1000);
  fakePortA = 0xFF;
  tick(1000);
  assert(currentFloor == 2 && systemState == SYS_DOORS_OPEN && req_internal[4]);
  tick(3000); tick(0); tick(2000); tick(2000);
  assert(currentFloor == 4 && systemState == SYS_DOORS_OPEN && !pending());
}

void testPWMUpdatesInTransit() {
  resetController();
  req_internal[5] = true;
  tick(0);
  assert(fakePWM[MOTOR_EN] == 150);
  fakeLoad = 1023;
  tick(100);
  assert(currentLoad == 350 && fakePWM[MOTOR_EN] == 255);
}

void testBoundaryGuards() {
  resetController(5); startMotor(UP);
  assert(systemState == SYS_IDLE && fakePWM[MOTOR_EN] == 0);
  resetController(1); startMotor(DOWN);
  assert(systemState == SYS_IDLE && fakePWM[MOTOR_EN] == 0);
}

void testTimerRollover() {
  if constexpr(sizeof(unsigned long) != 4) {
    std::cout << "SKIP: timer rollover requires Arduino-width 32-bit unsigned long.\n";
    return;
  }
  resetController();
  fakeNow = ULONG_MAX - 1000;
  req_internal[2] = true;
  tick(0); tick(1999);
  assert(currentFloor == 1);
  tick(1);
  assert(currentFloor == 2 && systemState == SYS_DOORS_OPEN);
}

void testSeededRequestSequence() {
  resetController();
  std::mt19937 rng(2841);
  for(int i = 0; i < 10000; ++i) {
    if(rng() % 20 == 0) {
      const int floor = 1 + rng() % 5;
      const int kind = rng() % 3;
      if(kind == 0) req_internal[floor] = true;
      else if(kind == 1 && floor < 5) req_up[floor] = true;
      else if(kind == 2 && floor > 1) req_down[floor] = true;
    }
    tick(250);
    if(systemState != SYS_TRANSIT) assert(fakePWM[MOTOR_EN] == 0);
  }
  for(int i = 0; i < 800; ++i) tick(250);
  assert(!pending() && systemState == SYS_IDLE && fakePWM[MOTOR_EN] == 0);
}

int main() {
  setup();
  testInputMapping();
  testArrivalClearingAndHeldButton();
  testOppositeCallAtTopAndBottom();
  testDirectionPriorityAndReturnService();
  testTurnaroundAtCurrentFloor();
  testRequestDuringTravel();
  testPWMUpdatesInTransit();
  testBoundaryGuards();
  testTimerRollover();
  testSeededRequestSequence();
  std::cout << (sizeof(unsigned long) == 4 ? 10 : 9)
            << " controller checks passed, including a 10,000-tick seeded request sequence.\n";
}
