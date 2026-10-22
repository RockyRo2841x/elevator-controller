#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>

#define MOTOR_EN  10
#define MOTOR_IN1 12
#define MOTOR_IN2 11
#define LOAD_PIN A0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_MCP23X17 mcp;

//Elevator Variables
int currentFloor = 1;
enum Direction { DOWN = -1, IDLE = 0, UP = 1 };
Direction currentDirection = IDLE;
int currentLoad = 0;
int basePWM = 0;

//Non-Blocking Variables
enum SysState { SYS_IDLE, SYS_TRANSIT, SYS_DOORS_OPEN };
SysState systemState = SYS_IDLE;
unsigned long stateTimer = 0;
const unsigned long TRANSIT_TIME = 2000; // 2 seconds between floors
const unsigned long DOOR_TIME = 3000;    // 3 seconds doors open

bool req_internal[6] = {false, false, false, false, false, false};
bool req_up[6]       = {false, false, false, false, false, false};
bool req_down[6]     = {false, false, false, false, false, false};

void readButtonStates(){
  static uint8_t previousA = 0xFF;
  static uint8_t previousB = 0xFF;
  const uint8_t currentA = mcp.readGPIOA();
  const uint8_t currentB = mcp.readGPIOB();
  const uint8_t pressedA = previousA & static_cast<uint8_t>(~currentA);
  const uint8_t pressedB = previousB & static_cast<uint8_t>(~currentB);
  previousA = currentA;
  previousB = currentB;

  //Internal Button Presses
  for(int i = 1; i <= 5; i++){
    if(pressedA & (1 << (8 - i)))
    req_internal[i] = true;
  }

  //External Buttons
  if(pressedB & (1 << 7))
  req_up[1]   = true;
  if(pressedB & (1 << 6))
  req_down[2] = true;
  if(pressedB & (1 << 5))
  req_up[2]   = true;
  if(pressedB & (1 << 4))
  req_down[3] = true;
  if(pressedB & (1 << 3))
  req_up[3]   = true;
  if(pressedB & (1 << 2))
  req_down[4] = true;
  if(pressedB & (1 << 1))
  req_up[4]   = true;
  if(pressedB & (1 << 0))
  req_down[5] = true;
}

bool requestsExistAbove();
bool requestsExistBelow();

bool shouldStopAtCurrentFloor(){
  if(req_internal[currentFloor]) 
    return true;
  if(currentDirection == UP && req_up[currentFloor]) 
    return true;
  if(currentDirection == DOWN && req_down[currentFloor]) 
    return true;
  if(currentDirection == IDLE && (req_up[currentFloor] || req_down[currentFloor])) 
    return true;
  
  // Serve an opposite-direction call at this direction's turnaround floor.
  if(currentDirection == UP && !requestsExistAbove() && req_down[currentFloor])
    return true;
  if(currentDirection == DOWN && !requestsExistBelow() && req_up[currentFloor])
    return true;
  return false;
}

void clearRequestsAtCurrentFloor(){
  req_internal[currentFloor] = false;
  if(currentDirection == UP || currentDirection == IDLE) 
  req_up[currentFloor] = false;
  if(currentDirection == DOWN || currentDirection == IDLE) 
  req_down[currentFloor] = false;
  const bool turnaround =
    (currentDirection == UP && !requestsExistAbove()) ||
    (currentDirection == DOWN && !requestsExistBelow());
  if(turnaround){
    req_up[currentFloor] = false;
    req_down[currentFloor] = false;
  }
}

bool requestsExistAbove(){
  for(int i = currentFloor + 1; i <= 5; i++){
    if(req_internal[i] || req_up[i] || req_down[i]) 
      return true;
  }
  return false;
}

bool requestsExistBelow(){
  for(int i = currentFloor - 1; i >= 1; i--){
    if(req_internal[i] || req_up[i] || req_down[i]) 
    return true;
  }
  return false;
}

void stopMotor();

void startMotor(Direction dir){
  // Prevent a timed leg beyond the simulated floor range.
  if((dir == UP && currentFloor >= 5) ||
     (dir == DOWN && currentFloor <= 1) || dir == IDLE){
    stopMotor();
    systemState = SYS_IDLE;
    currentDirection = IDLE;
    return;
  }
  if(dir == UP){
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  }else{
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
  }
  analogWrite(MOTOR_EN, basePWM); 
  
  systemState = SYS_TRANSIT;
  stateTimer = millis(); 
}

void stopMotor(){
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_EN, 0);
}

void calculateLoadAndPWM(){
  currentLoad = map(analogRead(LOAD_PIN), 0, 1023, 0, 350);
  basePWM = map(currentLoad, 0, 350, 150, 255);
  basePWM = constrain(basePWM, 150, 255);
}

void updateDisplay(String status){
  display.clearDisplay();
  
  display.setTextSize(2);
  display.setCursor(0,0);
  display.print("Floor: ");
  display.print(currentFloor);

  display.setTextSize(1);
  display.setCursor(0, 25);
  display.print("Status: ");
  display.print(status);

  display.setCursor(0, 40);
  display.print("Load: ");
  display.print(currentLoad);
  display.print(" kg");

  display.display();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(MOTOR_EN, OUTPUT);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(LOAD_PIN, INPUT);
  stopMotor();

  //Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  //Initialize MCP23017
  if(!mcp.begin_I2C(0x20)){
    Serial.println(F("MCP23017 initialization failed"));
    for(;;);
  }

  //Configure MCP Pins
  for(int i = 0; i < 16; i++){
    mcp.pinMode(i, INPUT_PULLUP);
  }

  updateDisplay("IDLE");
}

void loop() {
  calculateLoadAndPWM();
  readButtonStates();

  unsigned long currentTime = millis();

  if(systemState == SYS_DOORS_OPEN){
    updateDisplay("DOORS OPEN");
    if(currentTime - stateTimer >= DOOR_TIME)
      systemState = SYS_IDLE; 

  }else if(systemState == SYS_TRANSIT){
    analogWrite(MOTOR_EN, basePWM);
    updateDisplay((currentDirection == UP) ? "MOVING UP" : "MOVING DOWN");
    if (currentTime - stateTimer >= TRANSIT_TIME) {
      currentFloor = constrain(currentFloor + static_cast<int>(currentDirection), 1, 5);
      stateTimer = currentTime;       

      const bool forwardRequests = (currentDirection == UP)
        ? requestsExistAbove() : requestsExistBelow();
      if(shouldStopAtCurrentFloor() || !forwardRequests){
        stopMotor();
        clearRequestsAtCurrentFloor();
        systemState = SYS_DOORS_OPEN;
        stateTimer = currentTime;
      }
    }
  }else if(systemState == SYS_IDLE){
    String dirStr = (currentDirection == UP) ? "UP" : (currentDirection == DOWN) ? "DOWN" : "IDLE";
    updateDisplay(dirStr);

    if(shouldStopAtCurrentFloor()){
      clearRequestsAtCurrentFloor();
      systemState = SYS_DOORS_OPEN;
      stateTimer = currentTime;
    }else{
      if (currentDirection == UP){
        if (requestsExistAbove()) 
          startMotor(UP);
        else if (requestsExistBelow()){
          currentDirection = DOWN; 
          startMotor(DOWN);
        }else{
          currentDirection = IDLE; 
          stopMotor();
        }
      }else if(currentDirection == DOWN){
        if (requestsExistBelow()) 
          startMotor(DOWN);
        else if(requestsExistAbove()){
          currentDirection = UP; 
          startMotor(UP);
        }else{ 
          currentDirection = IDLE; 
          stopMotor(); 
        }
      }else{ 
        if(requestsExistAbove()){
          currentDirection = UP; 
          startMotor(UP);
        }else if(requestsExistBelow()){
          currentDirection = DOWN; 
          startMotor(DOWN);
        }
      }
    }
  }
}
