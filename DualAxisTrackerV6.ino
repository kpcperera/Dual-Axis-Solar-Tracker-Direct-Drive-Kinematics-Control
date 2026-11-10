#include <AccelStepper.h>
#include <Wire.h>
#include <SoftwareWire.h>

#define EN_PIN 8
#define FAN_PIN 9 // Plugs into X-Endstop pin for MOSFET logic (Option 2)

// CNC Shield V4 Pins
#define X_DIR  2
#define X_STEP 5
#define Y_DIR  3
#define Y_STEP 6

// LDR Analog Pins
#define LDR_TL A0
#define LDR_TR A1
#define LDR_BL A2
#define LDR_BR A3

// Software I2C for Tilt AS5600 (D11 = SDA, D12 = SCL)
SoftwareWire softWire(11, 12);

// Absolute Hardware Angle Limits
const int PAN_START  = 1810;
const int PAN_END    = 37;
const int TILT_START = 1250;
const int TILT_END   = 1080;

// Tracking Constants - Decoupled Deadbands
const int PAN_DEADBAND = 10;       // Tight accuracy for stable pan axis
const int TILT_DEADBAND = 40;      // Wider zone to absorb tilt mass and prevent hunting
const int NIGHT_THRESHOLD = 200;   
const int EVENING_DELAY = 10000;   

AccelStepper panMotor(1, X_STEP, X_DIR);
AccelStepper tiltMotor(1, Y_STEP, Y_DIR);

int getSmoothedReading(int pin) {
  long total = 0;
  for (int i = 0; i < 5; i++) {
    total += analogRead(pin);
    delay(2); 
  }
  return total / 5;
}

int getSmoothedPanAngle() {
  long total = 0;
  int count = 0;
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(0x36);
    Wire.write(0x0C);
    Wire.endTransmission();
    Wire.requestFrom(0x36, 2);
    if (Wire.available() == 2) {
      int angle = (Wire.read() << 8) | Wire.read();
      total += angle;
      count++;
    }
    delay(2);
  }
  return (count > 0) ? (total / count) : -1;
}

int getSmoothedTiltAngle() {
  long total = 0;
  int count = 0;
  for (int i = 0; i < 5; i++) {
    softWire.beginTransmission(0x36);
    softWire.write(0x0C);
    softWire.endTransmission();
    softWire.requestFrom(0x36, 2);
    if (softWire.available() == 2) {
      int angle = (softWire.read() << 8) | softWire.read();
      total += angle;
      count++;
    }
    delay(2);
  }
  return (count > 0) ? (total / count) : -1;
}

void setup() {
  Serial.begin(115200);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW); // Enable motors

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH); // Enable cooling fan

  Wire.begin();     
  softWire.begin(); 

  panMotor.setMinPulseWidth(20); 
  panMotor.setMaxSpeed(5);      
  panMotor.setAcceleration(5);   

  tiltMotor.setMinPulseWidth(20);
  tiltMotor.setMaxSpeed(5);
  tiltMotor.setAcceleration(5);

  Serial.println("Solar Tracker Initialized.");
}

void loop() {
  int tl = getSmoothedReading(LDR_TL);
  int tr = getSmoothedReading(LDR_TR);
  int bl = getSmoothedReading(LDR_BL);
  int br = getSmoothedReading(LDR_BR);

  int currentPanAngle = getSmoothedPanAngle();
  int currentTiltAngle = getSmoothedTiltAngle();

  int avgLeft = (tl + bl) / 2;
  int avgRight = (tr + br) / 2;
  int avgTop = (tl + tr) / 2;
  int avgBottom = (bl + br) / 2;
  int overallLight = (tl + tr + bl + br) / 4;

  int panDiff = avgLeft - avgRight;
  int tiltDiff = avgBottom - avgTop;

  // --- BOUNDARY OVERRIDE LOGIC ---
  const int TILT_BUFFER = 15;
  const int PAN_BUFFER = 15;
  
  if (currentTiltAngle != -1) {
    if (currentTiltAngle >= (TILT_START - TILT_BUFFER) && tiltDiff < 0) tiltDiff = 0; 
    if (currentTiltAngle <= (TILT_END + TILT_BUFFER) && tiltDiff > 0) tiltDiff = 0;   
  }
  
  if (currentPanAngle != -1) {
    if (currentPanAngle >= (PAN_START - PAN_BUFFER) && panDiff < 0) panDiff = 0; 
    if (currentPanAngle <= (PAN_END + PAN_BUFFER) && panDiff > 0) panDiff = 0;   
  }

  Serial.print("PanAng: "); Serial.print(currentPanAngle);
  Serial.print(" (Diff: "); Serial.print(panDiff);
  Serial.print(") | TiltAng: "); Serial.print(currentTiltAngle);
  Serial.print(" (Diff: "); Serial.print(tiltDiff);
  Serial.println(")");

  // Evening Return Logic
  if (overallLight < NIGHT_THRESHOLD) {
    digitalWrite(FAN_PIN, LOW); // Kill fan at night
    Serial.println("Sun lost. Waiting for evening delay...");
    delay(EVENING_DELAY);
    
    if (currentTiltAngle != -1 && currentTiltAngle < (TILT_START - TILT_BUFFER)) {
      tiltMotor.move(10); 
    }
    if (currentPanAngle != -1 && currentPanAngle < (PAN_START - PAN_BUFFER)) {
      panMotor.move(10); 
    }
  } 
  
  // Active Tracking Logic
  else {
    digitalWrite(FAN_PIN, HIGH); // Ensure fan runs during active tracking

    // Priority 1: Tilt calculation (Uses 40 deadband, reduced to 5-step burst to kill oscillation)
    if (abs(tiltDiff) > TILT_DEADBAND && currentTiltAngle != -1) {
      if (tiltDiff > 0 && currentTiltAngle > (TILT_END + TILT_BUFFER)) {
        tiltMotor.move(-5); 
      } else if (tiltDiff < 0 && currentTiltAngle < (TILT_START - TILT_BUFFER)) {
        tiltMotor.move(5);  
      }
    }

    // Priority 2: Pan calculation (Uses 10 deadband, strict "else if" for power management)
    else if (abs(panDiff) > PAN_DEADBAND && currentPanAngle != -1) {
      if (panDiff > 0 && currentPanAngle > (PAN_END + PAN_BUFFER)) {
        panMotor.move(-2); 
      } else if (panDiff < 0 && currentPanAngle < (PAN_START - PAN_BUFFER)) {
        panMotor.move(2);  
      }
    }
  }

  // Sequential Execution (Zero simultaneous power draw)
  while (tiltMotor.distanceToGo() != 0) {
    tiltMotor.run();
  } 
  
  while (panMotor.distanceToGo() != 0) {
    panMotor.run();
  }
}
