/*******************************************************
 * DistinctEyeShapes_EveStyle.ino
 *
 * ESP32 + SSD1306 "Floating Eyes," 
 * with visually distinct shapes for each emotion:
 *   - Happy: An upside-down "teardrop" arc (like EVE's smiling eye).
 *   - Neutral: Plain white circle.
 *   - Sad: A right-side-up "teardrop" arc (like EVE's sad eye).
 *   - Angry: (unchanged) A white bar with diagonal slash.
 *
 * No pupils. Blink logic: short line when eyes are closed.
 *******************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h> // Add Servo library for ESP32

// ----- WiFi Credentials -----
const char* ssid     = "RAY";
const char* password = "success@2020";

// ----- Display Settings -----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

// ----- Servo Settings -----
#define SERVO_PIN      13    // GPIO pin for servo control
#define SERVO_MIN_POS  0     // Minimum angle (left position)
#define SERVO_CENTER   90    // Center position
#define SERVO_MAX_POS  180   // Maximum angle (right position)
#define SERVO_MOVE_DELAY 15  // Delay between each degree of movement for smooth motion

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo headServo;              // Servo object for head movement

// ----- Web Server -----
WebServer server(80);

// ----- Global State -----
String currentState = "neutral";
bool   eyesOpen     = true;
bool   displayingText = false;
int    currentHeadPosition = SERVO_CENTER; // Track current head position

// Blinking intervals
unsigned long lastBlinkTime  = 0;
const unsigned long BLINK_INTERVAL = 3000; // ms
const unsigned long BLINK_DURATION = 200;  // ms

// Text display timing
unsigned long textDisplayStartTime = 0;
const int textDisplayDuration = 10000; // 10 seconds

// Forward declarations
void handleRoot();
void handleFaceRequest();
void handleHeadRequest();
void handleNotFound();
void displayText(const String &txt);
void updateFaceDisplay();
void moveHeadTo(int position);
void moveHeadLeft();
void moveHeadRight();
void moveHeadCenter();
void moveHeadLeftRight(); // Head shake animation

void drawHappyFace();
void drawNeutralFace();
void drawSadFace();
void drawAngryFace();
void drawGrinningFace();
void drawScaredFace();

void drawClosedEye(int cx, int cy, int width);

// "EVE-like" eye helpers
void drawEveHappyEye(int cx, int cy, int r);
void drawEveSadEye(int cx, int cy, int r);
void drawEveGrinningEye(int cx, int cy, int r);
void drawEveScaredEye(int cx, int cy, int r);

/**
 * Draw an angry eye - angled downward toward the center with circular bottom
 * Creating a furrowed, angry look
 */
void drawAngryEye(int cx, int cy, int width, int height) {
  // Create an angled eye that looks angry
  // Start with the basic coordinates
  int left = cx - width/2;
  int right = cx + width/2;
  int top = cy - height/2;
  int bottom = cy + height/2;
  int radius = height/2;
  
  // For angry eyes, we want them angled downward toward the center
  // Adjust the points to create the angry angle
  int innerOffset = 3; // How much the inner corner is lowered
  
  // Draw a filled circle for the base
  display.fillCircle(cx, cy, radius, SSD1306_WHITE);
  
  // Draw the angled top part (triangular) to create angry expression
  if (cx < SCREEN_WIDTH/2) {
    // Left eye - angle down toward right (inner side)
    // Create a triangle to cut the top-left part of the circle
    display.fillTriangle(
      left - 2, top - 2,      // Upper left (outside circle)
      left + width/3, top,    // Upper middle
      left - 2, top + height/2,  // Middle left
      SSD1306_BLACK
    );
    
    // Add a small line above the eye to indicate a furrowed brow
    display.drawLine(left, top - 2, right, top + innerOffset, SSD1306_WHITE);
    
  } else {
    // Right eye - angle down toward left (inner side)
    // Create a triangle to cut the top-right part of the circle
    display.fillTriangle(
      right + 2, top - 2,     // Upper right (outside circle)
      right - width/3, top,   // Upper middle
      right + 2, top + height/2, // Middle right
      SSD1306_BLACK
    );
    
    // Add a small line above the eye to indicate a furrowed brow
    display.drawLine(left, top + innerOffset, right, top - 2, SSD1306_WHITE);
  }
}

/*******************************************************
 * Setup & Loop
 *******************************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed!");
    while(true);
  }
  display.clearDisplay();
  display.display();
  Serial.println("OLED init OK.");
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  headServo.setPeriodHertz(50);    // Standard 50hz servo
  headServo.attach(SERVO_PIN, 500, 2400); // Attach to pin with min/max microsecond range
  moveHeadCenter(); // Center the head at startup
  Serial.println("Servo init OK.");

  // Connect WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // Show IP on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("EmoDeskBot");
  display.println("Connected!");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);

  // Start server
  server.on("/", handleRoot);
  server.on("/face", handleFaceRequest);
  server.on("/head", handleHeadRequest); // Add endpoint for head control
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started.");

  // Initial expression
  currentState = "neutral";
  updateFaceDisplay();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // If displaying text, check if time's up
  if(displayingText) {
    if(now - textDisplayStartTime > textDisplayDuration) {
      displayingText = false;
      updateFaceDisplay(); // revert to face
    }
    return; // skip blinking logic while text is up
  }

  // Blinking logic
  if(eyesOpen && (now - lastBlinkTime > BLINK_INTERVAL)) {
    eyesOpen = false;  // close eyes
    lastBlinkTime = now;
    updateFaceDisplay();
  }
  else if(!eyesOpen && (now - lastBlinkTime > BLINK_DURATION)) {
    eyesOpen = true;   // open eyes
    updateFaceDisplay();
  }
}

/*******************************************************
 * Web Handlers
 *******************************************************/
void handleRoot() {
  String page = "<html><head><title>EmoDeskBot</title></head><body>";
  page += "<h1>EmoDeskBot Control</h1>";
  page += "<p>Current state: " + currentState + "</p>";
  page += "<a href='/face?state=happy'><button>Happy</button></a> ";
  page += "<a href='/face?state=neutral'><button>Neutral</button></a> ";
  page += "<a href='/face?state=sad'><button>Sad</button></a> ";
  page += "<a href='/face?state=angry'><button>Angry</button></a> ";
  page += "<a href='/face?state=grinning'><button>Grinning</button></a> ";
  page += "<a href='/face?state=scared'><button>Scared</button></a> ";
  page += "<br><br>";
  page += "<a href='/head?pos=left'><button>Head Left</button></a> ";
  page += "<a href='/head?pos=center'><button>Head Center</button></a> ";
  page += "<a href='/head?pos=right'><button>Head Right</button></a> ";
  page += "<a href='/head?pos=shake'><button>Head Shake</button></a> ";
  page += "<br><br><form action='/face' method='get'>";
  page += "Custom Text: <input type='text' name='state' value='text:Hello!'>";
  page += "<input type='submit' value='Show'>";
  page += "</form></body></html>";
  server.send(200, "text/html", page);
}

void handleFaceRequest() {
  if(!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing state parameter");
    return;
  }

  String s = server.arg("state");
  currentState = s;
  Serial.println("New state: " + s);

  if(s == "happy") {
    drawHappyFace();
    server.send(200, "text/plain", "Showing HAPPY face");
  }
  else if(s == "neutral") {
    drawNeutralFace();
    server.send(200, "text/plain", "Showing NEUTRAL face");
  }
  else if(s == "sad") {
    drawSadFace();
    server.send(200, "text/plain", "Showing SAD face");
  }
  else if(s == "angry") {
    drawAngryFace();
    server.send(200, "text/plain", "Showing ANGRY face");
  }
  else if(s == "grinning") {
    drawGrinningFace();
    server.send(200, "text/plain", "Showing GRINNING face");
  }
  else if(s == "scared") {
    drawScaredFace();
    server.send(200, "text/plain", "Showing SCARED face");
  }
  else if(s.startsWith("text:")) {
    String content = s.substring(5);
    displayText(content);
    server.send(200, "text/plain", "Showing text: " + content);
  }
  else {
    server.send(400, "text/plain", "Invalid state");
  }
}

void handleHeadRequest() {
  if(!server.hasArg("pos")) {
    server.send(400, "text/plain", "Missing position parameter");
    return;
  }

  String pos = server.arg("pos");
  Serial.println("Head position: " + pos);

  if(pos == "left") {
    moveHeadLeft();
    server.send(200, "text/plain", "Head moved left");
  }
  else if(pos == "right") {
    moveHeadRight();
    server.send(200, "text/plain", "Head moved right");
  }
  else if(pos == "center") {
    moveHeadCenter();
    server.send(200, "text/plain", "Head centered");
  }
  else if(pos == "shake") {
    moveHeadLeftRight();
    server.send(200, "text/plain", "Head shaking");
  }
  else if(pos.startsWith("angle:")) {
    // Handle specific angle if needed
    String angleStr = pos.substring(6);
    int angle = angleStr.toInt();
    if(angle >= SERVO_MIN_POS && angle <= SERVO_MAX_POS) {
      moveHeadTo(angle);
      server.send(200, "text/plain", "Head moved to " + angleStr + " degrees");
    } else {
      server.send(400, "text/plain", "Invalid angle");
    }
  }
  else {
    server.send(400, "text/plain", "Invalid position");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

/*******************************************************
 * Display Text
 *******************************************************/
void displayText(const String &txt){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  int charCount=0, lineCount=0, maxLines=6;

  for(int i=0; i<txt.length() && lineCount<maxLines; i++){
    char c = txt.charAt(i);
    if(c=='\n'){
      display.println();
      charCount=0;
      lineCount++;
    } else {
      if(charCount>=21){
        display.println();
        charCount=0;
        lineCount++;
        if(lineCount>=maxLines) break;
      }
      display.print(c);
      charCount++;
    }
  }

  display.display();
  displayingText = true;
  textDisplayStartTime = millis();
}

/*******************************************************
 * Update Face
 *******************************************************/
void updateFaceDisplay(){
  if(currentState == "happy")       drawHappyFace();
  else if(currentState == "sad")    drawSadFace();
  else if(currentState == "angry")  drawAngryFace();
  else if(currentState == "grinning") drawGrinningFace();
  else if(currentState == "scared") drawScaredFace();
  else                              drawNeutralFace();
}

/*******************************************************
 * Closed Eye
 * Just a short horizontal line
 *******************************************************/
void drawClosedEye(int cx, int cy, int width){
  int halfW = width/2;
  display.drawLine(cx - halfW, cy, cx + halfW, cy, SSD1306_WHITE);
}

/*******************************************************
 * Eye Shapes (EVE-like, No Pupils)
 *******************************************************/

/**
 * EVE HAPPY eye:
 *  Fill a circle shifted UP, then black out its BOTTOM portion,
 *  so the visible arc is on the UPPER side.
 */
void drawEveHappyEye(int cx, int cy, int r) {
  // Shift circle upward slightly
  int offset = 3;
  int cyShifted = cy - offset;

  // Fill the circle
  display.fillCircle(cx, cyShifted, r, SSD1306_WHITE);

  // Black out the bottom portion, leaving a top arc visible
  // Make the cut closer to the top to create a thinner, cleaner arc
  int cutHeight = r - 1; 
  display.fillRect(cx - r, cyShifted, 2*r, cutHeight + 2, SSD1306_BLACK);
  
  // Draw a small line at the bottom of the eyes for a more expressive happy look
  int lineWidth = r;
  display.drawLine(cx - lineWidth/2, cyShifted + 2, cx + lineWidth/2, cyShifted + 2, SSD1306_WHITE);
}

/**
 * EVE SAD eye:
 *  Fill a circle shifted DOWN, then black out its TOP portion,
 *  so the visible arc is on the LOWER side.
 */
void drawEveSadEye(int cx, int cy, int r) {
  // Shift circle downward slightly
  int offset = 3;
  int cyShifted = cy + offset;

  // Fill the circle
  display.fillCircle(cx, cyShifted, r, SSD1306_WHITE);

  // Black out the top portion, leaving a bottom arc visible
  int cutHeight = r - 1;
  display.fillRect(cx - r, cyShifted - r, 2*r, cutHeight, SSD1306_BLACK);
  
  // Add a small teardrop beneath the eye for a more expressive sad face
  display.fillCircle(cx - 3, cyShifted + r + 3, 2, SSD1306_WHITE);
}

/**
 * EVE GRINNING eye:
 * Rectangular/square eyes with teeth below as shown in the image
 */
void drawEveGrinningEye(int cx, int cy, int r) {
  // Draw a perfect square eye (exactly matching the image)
  int eyeSize = 12; // Perfect square eye
  
  // Draw filled square for each eye
  display.fillRect(cx - eyeSize/2, cy - eyeSize/2, eyeSize, eyeSize, SSD1306_WHITE);
}

/**
 * EVE SCARED eye:
 *  Large circular eyes to represent fear/surprise
 */
void drawEveScaredEye(int cx, int cy, int r) {
  // Draw a larger circle than neutral for surprised/scared look
  int scaredRadius = r + 2; // Make eyes slightly larger for scared effect
  
  // Draw filled circle for each eye
  display.fillCircle(cx, cy, scaredRadius, SSD1306_WHITE);
  
  // Add small highlight (empty circle inside) to enhance scared look
  display.fillCircle(cx, cy, scaredRadius - 4, SSD1306_BLACK);
}

/*******************************************************
 * Face Functions
 *******************************************************/
void drawHappyFace(){
  display.clearDisplay();

  // Eye coordinates - increase spacing between eyes
  int leftEyeX  = 42;  // Moved left (was 48)
  int rightEyeX = 86;  // Moved right (was 80)
  int eyeY      = SCREEN_HEIGHT / 2;
  int eyeRadius = 12;

  if(eyesOpen){
    // Use the EVE-like "happy" shape
    drawEveHappyEye(leftEyeX,  eyeY, eyeRadius);
    drawEveHappyEye(rightEyeX, eyeY, eyeRadius);
  } else {
    // Closed eye line
    drawClosedEye(leftEyeX,  eyeY, eyeRadius*2);
    drawClosedEye(rightEyeX, eyeY, eyeRadius*2);
  }
  display.display();
}

void drawNeutralFace(){
  display.clearDisplay();

  // Increase spacing between eyes
  int leftEyeX  = 44;  // Was 50
  int rightEyeX = 84;  // Was 78
  int eyeY      = SCREEN_HEIGHT / 2;
  int eyeRadius = 10;

  if(eyesOpen){
    // Plain white circle
    display.fillCircle(leftEyeX,  eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
  } else {
    // Closed lines
    drawClosedEye(leftEyeX,  eyeY, eyeRadius*2);
    drawClosedEye(rightEyeX, eyeY, eyeRadius*2);
  }
  display.display();
}

void drawSadFace(){
  display.clearDisplay();

  // Increase spacing between eyes
  int leftEyeX  = 44;  // Was 50
  int rightEyeX = 84;  // Was 78
  int eyeY      = SCREEN_HEIGHT / 2;
  int eyeRadius = 10;

  if(eyesOpen){
    // Use the EVE-like "sad" shape
    drawEveSadEye(leftEyeX,  eyeY, eyeRadius);
    drawEveSadEye(rightEyeX, eyeY, eyeRadius);
  } else {
    drawClosedEye(leftEyeX,  eyeY, eyeRadius*2);
    drawClosedEye(rightEyeX, eyeY, eyeRadius*2);
  }
  display.display();
}

void drawAngryFace(){
  display.clearDisplay();

  // Increase spacing between eyes
  int leftEyeX  = 42;
  int rightEyeX = 86;
  int eyeY      = SCREEN_HEIGHT / 2;
  int eyeWidth  = 20;
  int eyeHeight = 12;

  if(eyesOpen){
    // New improved angry eyes
    drawAngryEye(leftEyeX, eyeY, eyeWidth, eyeHeight);
    drawAngryEye(rightEyeX, eyeY, eyeWidth, eyeHeight);
  } else {
    // Closed lines
    drawClosedEye(leftEyeX, eyeY, eyeWidth);
    drawClosedEye(rightEyeX, eyeY, eyeWidth);
  }
  display.display();
}

void drawGrinningFace(){
  display.clearDisplay();

  // Eye coordinates
  int leftEyeX  = 42;
  int rightEyeX = 86;
  int eyeY      = SCREEN_HEIGHT / 2 - 8; // Move eyes higher to match image exactly
  int eyeRadius = 10;
  
  // Teeth parameters - adjusted to match image exactly
  int teethTop = SCREEN_HEIGHT / 2 + 8;
  int teethWidth = 8;  // Wider teeth to match image
  int teethHeight = 8; // Height of teeth
  int numTeeth = 5;    // Exactly 5 teeth as shown in image
  int teethStart = SCREEN_WIDTH/2 - (numTeeth * teethWidth)/2;
  int teethGap = 2;    // Small gap between teeth for better visual

  // Always draw closed eyes for grinning face (no eyesOpen check)
  int eyeWidth = 12;
  
  // Left eye - closed dash with slight angle
  display.drawLine(leftEyeX - eyeWidth/2, eyeY - 1, leftEyeX + eyeWidth/2, eyeY + 1, SSD1306_WHITE);
  
  // Right eye - closed dash with slight angle
  display.drawLine(rightEyeX - eyeWidth/2, eyeY - 1, rightEyeX + eyeWidth/2, eyeY + 1, SSD1306_WHITE);
  
  // Draw teeth
  for (int i = 0; i < numTeeth; i++) {
    int toothX = teethStart + (i * (teethWidth + teethGap));
    display.fillRect(toothX, teethTop, teethWidth, teethHeight, SSD1306_WHITE);
  }
  
  display.display();
}

void drawScaredFace(){
  display.clearDisplay();

  // Eye coordinates similar to the other faces
  int leftEyeX  = 42;
  int rightEyeX = 86; 
  int eyeY      = SCREEN_HEIGHT / 2 - 2; // Slightly higher on screen
  int eyeRadius = 10;

  if(eyesOpen){
    // Draw the scared eyes
    drawEveScaredEye(leftEyeX, eyeY, eyeRadius);
    drawEveScaredEye(rightEyeX, eyeY, eyeRadius);
    
    // Draw a curved open mouth that matches the image (inward curves at top and bottom)
    int mouthCenterX = SCREEN_WIDTH/2;
    int mouthCenterY = SCREEN_HEIGHT/2 + 14; // Lower position for the mouth
    int mouthWidth = 20;  // Width of the mouth
    int mouthHeight = 10; // Height of the mouth
    
    // Draw the curved mouth shape
    // Top curved line (curved inward at the middle)
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY - mouthHeight/2, // Left top
                     mouthCenterX - mouthWidth/4, mouthCenterY - mouthHeight/2 + 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX - mouthWidth/4, mouthCenterY - mouthHeight/2 + 2,
                     mouthCenterX + mouthWidth/4, mouthCenterY - mouthHeight/2 + 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/4, mouthCenterY - mouthHeight/2 + 2,
                     mouthCenterX + mouthWidth/2, mouthCenterY - mouthHeight/2, SSD1306_WHITE);
                     
    // Bottom curved line (curved inward at the middle)
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY + mouthHeight/2, // Left bottom
                     mouthCenterX - mouthWidth/4, mouthCenterY + mouthHeight/2 - 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX - mouthWidth/4, mouthCenterY + mouthHeight/2 - 2,
                     mouthCenterX + mouthWidth/4, mouthCenterY + mouthHeight/2 - 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/4, mouthCenterY + mouthHeight/2 - 2,
                     mouthCenterX + mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
                     
    // Vertical sides to connect the top and bottom curves
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY - mouthHeight/2, // Left side
                     mouthCenterX - mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/2, mouthCenterY - mouthHeight/2, // Right side
                     mouthCenterX + mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
  } else {
    // When eyes are closed, show trembling lines
    int lineWidth = eyeRadius * 2;
    // Wavy/trembling line for left eye
    display.drawLine(leftEyeX - lineWidth/2, eyeY - 1, 
                     leftEyeX - lineWidth/4, eyeY + 1, SSD1306_WHITE);
    display.drawLine(leftEyeX - lineWidth/4, eyeY + 1, 
                     leftEyeX + lineWidth/4, eyeY - 1, SSD1306_WHITE);
    display.drawLine(leftEyeX + lineWidth/4, eyeY - 1, 
                     leftEyeX + lineWidth/2, eyeY + 1, SSD1306_WHITE);
    
    // Wavy/trembling line for right eye
    display.drawLine(rightEyeX - lineWidth/2, eyeY - 1, 
                     rightEyeX - lineWidth/4, eyeY + 1, SSD1306_WHITE);
    display.drawLine(rightEyeX - lineWidth/4, eyeY + 1, 
                     rightEyeX + lineWidth/4, eyeY - 1, SSD1306_WHITE);
    display.drawLine(rightEyeX + lineWidth/4, eyeY - 1, 
                     rightEyeX + lineWidth/2, eyeY + 1, SSD1306_WHITE);
                     
    // Also draw the curved mouth when eyes are closed
    int mouthCenterX = SCREEN_WIDTH/2;
    int mouthCenterY = SCREEN_HEIGHT/2 + 14; // Lower position for the mouth
    int mouthWidth = 20;  // Width of the mouth
    int mouthHeight = 10; // Height of the mouth
    
    // Draw the curved mouth shape
    // Top curved line (curved inward at the middle)
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY - mouthHeight/2, // Left top
                     mouthCenterX - mouthWidth/4, mouthCenterY - mouthHeight/2 + 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX - mouthWidth/4, mouthCenterY - mouthHeight/2 + 2,
                     mouthCenterX + mouthWidth/4, mouthCenterY - mouthHeight/2 + 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/4, mouthCenterY - mouthHeight/2 + 2,
                     mouthCenterX + mouthWidth/2, mouthCenterY - mouthHeight/2, SSD1306_WHITE);
                     
    // Bottom curved line (curved inward at the middle)
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY + mouthHeight/2, // Left bottom
                     mouthCenterX - mouthWidth/4, mouthCenterY + mouthHeight/2 - 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX - mouthWidth/4, mouthCenterY + mouthHeight/2 - 2,
                     mouthCenterX + mouthWidth/4, mouthCenterY + mouthHeight/2 - 2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/4, mouthCenterY + mouthHeight/2 - 2,
                     mouthCenterX + mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
                     
    // Vertical sides to connect the top and bottom curves
    display.drawLine(mouthCenterX - mouthWidth/2, mouthCenterY - mouthHeight/2, // Left side
                     mouthCenterX - mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
    display.drawLine(mouthCenterX + mouthWidth/2, mouthCenterY - mouthHeight/2, // Right side
                     mouthCenterX + mouthWidth/2, mouthCenterY + mouthHeight/2, SSD1306_WHITE);
  }
  
  display.display();
}

/*******************************************************
 * Servo Control Functions
 *******************************************************/

// Move head to specific position smoothly
void moveHeadTo(int position) {
  // Constrain position to valid range
  position = constrain(position, SERVO_MIN_POS, SERVO_MAX_POS);
  
  // Move smoothly - step by step
  if(position > currentHeadPosition) {
    // Moving right
    for(int pos = currentHeadPosition; pos <= position; pos++) {
      headServo.write(pos);
      delay(SERVO_MOVE_DELAY);
    }
  } else if(position < currentHeadPosition) {
    // Moving left
    for(int pos = currentHeadPosition; pos >= position; pos--) {
      headServo.write(pos);
      delay(SERVO_MOVE_DELAY);
    }
  }
  
  // Update current position
  currentHeadPosition = position;
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
  currentHeadPosition = SERVO_CENTER;
}
