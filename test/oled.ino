/*******************************************************
 * Cute Cartoon Face with Blinking Eyes + Gentle Eyebrows
 * for ESP32 + SSD1306 (128x64) I2C OLED
 *
 * Wiring:
 *   OLED VCC -> 3.3V
 *   OLED GND -> GND
 *   OLED SCL -> GPIO 22
 *   OLED SDA -> GPIO 21
 *******************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET   -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Animation parameters
// Try smaller or larger for eyes/offsets if you want a different look
#define EYE_OFFSET_X    14   // Horizontal offset for eyes from face center
#define EYE_OFFSET_Y     8   // Vertical offset from face center to eyes
#define EYE_RADIUS       4   // Eye size
#define BLINK_INTERVAL 3000  // ms between blinks
#define BLINK_DURATION  200  // ms eyes stay closed

// Positioning constants
#define FACE_CENTER_X   (SCREEN_WIDTH / 2)
#define FACE_CENTER_Y   (SCREEN_HEIGHT / 2)

// Eyebrow positioning relative to eye center
// We'll place eyebrows ~6px above each eye, each ~10px wide
#define EYEBROW_OFFSET_Y  6
#define EYEBROW_HALF_WIDTH  5
#define EYEBROW_ANGLE  2    // vertical shift to tilt the eyebrow slightly

unsigned long lastBlinkTime = 0;
bool eyesOpen = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing OLED...");

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed. Check wiring!");
    while(true);
  }

  display.clearDisplay();
  display.display();
  
  Serial.println("OLED init complete!");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Trigger a blink after BLINK_INTERVAL
  if (eyesOpen && (currentTime - lastBlinkTime > BLINK_INTERVAL)) {
    eyesOpen = false; // Close eyes
    lastBlinkTime = currentTime;
  } 
  // Re-open eyes after BLINK_DURATION
  else if (!eyesOpen && (currentTime - lastBlinkTime > BLINK_DURATION)) {
    eyesOpen = true;
  }

  drawCuteFace();
  delay(50);
}

void drawCuteFace() {
  display.clearDisplay();

  // --- EYES ---
  int leftEyeX  = FACE_CENTER_X - EYE_OFFSET_X;
  int rightEyeX = FACE_CENTER_X + EYE_OFFSET_X;
  int eyeY      = FACE_CENTER_Y - EYE_OFFSET_Y;

  if (eyesOpen) {
    // Filled circle eyes (slightly bigger for a cartoon look)
    display.fillCircle(leftEyeX,  eyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyeY, EYE_RADIUS, SSD1306_WHITE);

    // A small black dot offset inside each eye for "pupil/shine"
    display.fillCircle(leftEyeX + 1,  eyeY - 1, 1, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 1, eyeY - 1, 1, SSD1306_BLACK);

  } else {
    // Draw closed eyes as short horizontal lines
    int halfLine = 5;
    display.drawLine(leftEyeX  - halfLine, eyeY, leftEyeX  + halfLine, eyeY, SSD1306_WHITE);
    display.drawLine(rightEyeX - halfLine, eyeY, rightEyeX + halfLine, eyeY, SSD1306_WHITE);
  }

  // --- EYEBROWS ---
  // We'll place each eyebrow ~6 pixels above its respective eye.
  drawEyebrow(leftEyeX,  eyeY - EYEBROW_OFFSET_Y,  true);
  drawEyebrow(rightEyeX, eyeY - EYEBROW_OFFSET_Y, false);

  // --- MOUTH (simple smile arc) ---
  // We'll shift it slightly down from face center
  int mouthRadius = 10;
  int mouthOffset = 10; 
  // Bottom half circle mask: 4 (bottom-left) + 8 (bottom-right) = 12
  display.drawCircleHelper(FACE_CENTER_X, FACE_CENTER_Y + mouthOffset,
                           mouthRadius, 4 | 8, SSD1306_WHITE);

  display.display();
}

/**
 * Draw a gentle, angled eyebrow at (centerX, centerY).
 * For the left eyebrow, we angle upward to the left.
 * For the right eyebrow, we angle upward to the right.
 */
void drawEyebrow(int centerX, int centerY, bool leftSide) {
  // We'll define a half-width for the line, and a small vertical offset
  // to angle it slightly. You can tweak these for your taste.
  int halfWidth = EYEBROW_HALF_WIDTH;  // horizontal half-length of eyebrow
  int verticalTilt = EYEBROW_ANGLE;    // how many pixels up/down at ends

  int x1 = centerX - halfWidth;
  int x2 = centerX + halfWidth;
  int y1 = centerY;
  int y2 = centerY;

  if (leftSide) {
    // Slight upward angle to the outside
    y1 += verticalTilt;  
  } else {
    // For right eyebrow, tilt the other way
    y2 += verticalTilt;
  }

  display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
}
