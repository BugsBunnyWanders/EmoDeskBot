/*******************************************************
 * ESP32 Servo Test
 * 
 * A simple test to verify that the servo motor is working correctly
 * with your ESP32 before integrating with the EmoDeskBot.
 * 
 * This sketch will:
 * 1. Sweep the servo from left to right and back continuously
 * 2. Allow control via Serial monitor (commands: left, right, center, shake)
 * 
 * Hardware:
 * - Connect servo signal wire to GPIO 13 (can be changed below)
 * - Make sure servo is properly powered (5V and GND)
 *******************************************************/

#include <ESP32Servo.h>

// ----- Servo Settings -----
#define SERVO_PIN      13    // GPIO pin for servo control
#define SERVO_MIN_POS  0     // Minimum angle (left position)
#define SERVO_CENTER   90    // Center position
#define SERVO_MAX_POS  180   // Maximum angle (right position)
#define SERVO_MOVE_DELAY 15  // Delay between each degree of movement for smooth motion

// Create servo object
Servo headServo;

// Track current position
int currentPosition = SERVO_CENTER;

// Flag to control auto-sweep mode
bool autoSweepMode = true;

// Timing for smooth movements
unsigned long lastMoveTime = 0;
int sweepDirection = 1; // 1 = increasing angle, -1 = decreasing angle

void setup() {
  // Initialize serial for debugging and control
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP32 Servo Test");
  Serial.println("------------------");
  Serial.println("Commands: left, right, center, shake, sweep, stop");
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  headServo.setPeriodHertz(50);    // Standard 50hz servo
  headServo.attach(SERVO_PIN, 500, 2400); // Attach to pin with min/max microsecond range
  
  // Center the servo at startup
  moveHeadTo(SERVO_CENTER);
  Serial.println("Servo initialized at center position");
  Serial.println("Auto-sweep mode enabled. Send 'stop' to disable.");
}

void loop() {
  // Check for serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "left") {
      autoSweepMode = false;
      moveHeadLeft();
      Serial.println("Moving to left position");
    } 
    else if (command == "right") {
      autoSweepMode = false;
      moveHeadRight();
      Serial.println("Moving to right position");
    } 
    else if (command == "center") {
      autoSweepMode = false;
      moveHeadCenter();
      Serial.println("Moving to center position");
    } 
    else if (command == "shake") {
      autoSweepMode = false;
      moveHeadLeftRight();
      Serial.println("Performing head shake");
    }
    else if (command == "sweep") {
      autoSweepMode = true;
      Serial.println("Auto-sweep mode enabled");
    }
    else if (command == "stop") {
      autoSweepMode = false;
      Serial.println("Auto-sweep mode disabled");
    }
    else {
      // Try to parse as a number for direct angle control
      int angle = command.toInt();
      if (angle >= 0 && angle <= 180) {
        autoSweepMode = false;
        moveHeadTo(angle);
        Serial.println("Moving to angle: " + String(angle));
      } else {
        Serial.println("Unknown command: " + command);
        Serial.println("Available commands: left, right, center, shake, sweep, stop, or a number 0-180");
      }
    }
  }
  
  // Auto-sweep mode (if enabled)
  if (autoSweepMode) {
    unsigned long currentTime = millis();
    if (currentTime - lastMoveTime >= SERVO_MOVE_DELAY) {
      lastMoveTime = currentTime;
      
      // Update position based on direction
      currentPosition += sweepDirection;
      
      // Check if we reached min/max and reverse direction
      if (currentPosition >= SERVO_MAX_POS) {
        currentPosition = SERVO_MAX_POS;
        sweepDirection = -1;
        Serial.println("Reversing direction at max");
      } 
      else if (currentPosition <= SERVO_MIN_POS) {
        currentPosition = SERVO_MIN_POS;
        sweepDirection = 1;
        Serial.println("Reversing direction at min");
      }
      
      // Move servo to new position
      headServo.write(currentPosition);
    }
  }
}

// Move head to specific position smoothly
void moveHeadTo(int position) {
  // Constrain position to valid range
  position = constrain(position, SERVO_MIN_POS, SERVO_MAX_POS);
  
  // Move smoothly - step by step
  if (position > currentPosition) {
    // Moving right
    for (int pos = currentPosition; pos <= position; pos++) {
      headServo.write(pos);
      delay(SERVO_MOVE_DELAY);
    }
  } else if (position < currentPosition) {
    // Moving left
    for (int pos = currentPosition; pos >= position; pos--) {
      headServo.write(pos);
      delay(SERVO_MOVE_DELAY);
    }
  }
  
  // Update current position
  currentPosition = position;
}

// Move head to left position
void moveHeadLeft() {
  moveHeadTo(SERVO_MIN_POS);
}

// Move head to center position
void moveHeadCenter() {
  moveHeadTo(SERVO_CENTER);
}

// Move head to right position
void moveHeadRight() {
  moveHeadTo(SERVO_MAX_POS);
}

// Head shake animation (left-right-center)
void moveHeadLeftRight() {
  // Quick movements for shaking
  headServo.write(SERVO_MIN_POS + 20);
  delay(200);
  headServo.write(SERVO_MAX_POS - 20);
  delay(200);
  headServo.write(SERVO_MIN_POS + 20);
  delay(200);
  headServo.write(SERVO_MAX_POS - 20);
  delay(200);
  
  // Return to center
  moveHeadCenter();
  
  // Update position tracking
  currentPosition = SERVO_CENTER;
} 