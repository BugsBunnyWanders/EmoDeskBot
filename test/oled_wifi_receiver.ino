/*******************************************************
 * EmoDeskBot - ESP32 + OLED Display with WiFi Control
 * 
 * Receives facial expression commands via WiFi and displays
 * appropriate face on OLED screen.
 * 
 * Wiring:
 *   OLED VCC -> 3.3V
 *   OLED GND -> GND
 *   OLED SCL -> GPIO 22
 *   OLED SDA -> GPIO 21
 *******************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

// WiFi Credentials
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your WiFi name
const char* password = "YOUR_WIFI_PASS";  // Replace with your WiFi password

// Web server on port 80
WebServer server(80);

// OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Animation parameters
#define EYE_OFFSET_X    14   // Horizontal offset for eyes from face center
#define EYE_OFFSET_Y     8   // Vertical offset from face center to eyes
#define EYE_RADIUS       4   // Eye size
#define BLINK_INTERVAL 3000  // ms between blinks
#define BLINK_DURATION  200  // ms eyes stay closed

// Face center positions
#define FACE_CENTER_X   (SCREEN_WIDTH / 2)
#define FACE_CENTER_Y   (SCREEN_HEIGHT / 2)

// Eyebrow positioning relative to eye center
#define EYEBROW_OFFSET_Y  6
#define EYEBROW_HALF_WIDTH  5
#define EYEBROW_ANGLE  2    // vertical shift for eyebrow tilt

// State variables
unsigned long lastBlinkTime = 0;
bool eyesOpen = true;
String faceState = "neutral"; // "happy" or "neutral"

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("EmoDeskBot starting...");
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("EmoDeskBot");
  display.println("Connecting to WiFi...");
  display.display();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to WiFi! IP address: ");
    Serial.println(WiFi.localIP());
    
    // Set up the web server endpoints
    server.on("/face", HTTP_GET, handleFaceRequest);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");
    
    // Display the IP address on the OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("EmoDeskBot ready!");
    display.println("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(3000);
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi. Check your credentials.");
    
    // Show error on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi connection failed");
    display.println("Check credentials");
    display.display();
  }
}

void loop() {
  // Handle incoming HTTP requests
  server.handleClient();
  
  // Handle blinking animation
  unsigned long currentTime = millis();
  if (eyesOpen && (currentTime - lastBlinkTime > BLINK_INTERVAL)) {
    eyesOpen = false;
    lastBlinkTime = currentTime;
  } 
  else if (!eyesOpen && (currentTime - lastBlinkTime > BLINK_DURATION)) {
    eyesOpen = true;
  }
  
  // Draw the appropriate face
  if (faceState == "happy") {
    drawHappyFace();
  } else {
    drawNeutralFace();
  }
  
  delay(50);
}

// HTTP request handler for face state
void handleFaceRequest() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "happy" || state == "neutral") {
      faceState = state;
      Serial.println("Face state set to: " + faceState);
      server.send(200, "text/plain", "OK: Face state set to " + faceState);
    } else {
      server.send(400, "text/plain", "Invalid state. Use 'happy' or 'neutral'");
    }
  } else {
    server.send(400, "text/plain", "Missing 'state' parameter");
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

void drawHappyFace() {
  display.clearDisplay();

  // Draw eyes
  int leftEyeX  = FACE_CENTER_X - EYE_OFFSET_X;
  int rightEyeX = FACE_CENTER_X + EYE_OFFSET_X;
  int eyeY      = FACE_CENTER_Y - EYE_OFFSET_Y;

  if (eyesOpen) {
    // Open eyes with mini highlight
    display.fillCircle(leftEyeX, eyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyeY, EYE_RADIUS, SSD1306_WHITE);
    
    // Small reflection in eyes
    display.fillCircle(leftEyeX + 1, eyeY - 1, 1, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 1, eyeY - 1, 1, SSD1306_BLACK);
  } else {
    // Closed eyes as horizontal lines
    int halfLine = EYE_RADIUS + 1;
    display.drawLine(leftEyeX - halfLine, eyeY, leftEyeX + halfLine, eyeY, SSD1306_WHITE);
    display.drawLine(rightEyeX - halfLine, eyeY, rightEyeX + halfLine, eyeY, SSD1306_WHITE);
  }

  // Draw happy eyebrows (slightly raised at outside)
  drawEyebrow(leftEyeX, eyeY - EYEBROW_OFFSET_Y, true);
  drawEyebrow(rightEyeX, eyeY - EYEBROW_OFFSET_Y, false);

  // Draw big smile (wider mouth than neutral)
  int mouthRadius = 10;
  int mouthOffset = 10; 
  // Use 12 (4|8) for bottom half circle (4=bottom-left, 8=bottom-right)
  display.drawCircleHelper(FACE_CENTER_X, FACE_CENTER_Y + mouthOffset,
                           mouthRadius, 4 | 8, SSD1306_WHITE);

  display.display();
}

void drawNeutralFace() {
  display.clearDisplay();

  // Draw eyes
  int leftEyeX  = FACE_CENTER_X - EYE_OFFSET_X;
  int rightEyeX = FACE_CENTER_X + EYE_OFFSET_X;
  int eyeY      = FACE_CENTER_Y - EYE_OFFSET_Y;

  if (eyesOpen) {
    // Open eyes
    display.fillCircle(leftEyeX, eyeY, EYE_RADIUS, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyeY, EYE_RADIUS, SSD1306_WHITE);
    
    // Small reflection in eyes
    display.fillCircle(leftEyeX + 1, eyeY - 1, 1, SSD1306_BLACK);
    display.fillCircle(rightEyeX + 1, eyeY - 1, 1, SSD1306_BLACK);
  } else {
    // Closed eyes
    int halfLine = EYE_RADIUS + 1;
    display.drawLine(leftEyeX - halfLine, eyeY, leftEyeX + halfLine, eyeY, SSD1306_WHITE);
    display.drawLine(rightEyeX - halfLine, eyeY, rightEyeX + halfLine, eyeY, SSD1306_WHITE);
  }

  // Draw straight eyebrows
  int eyebrowY = eyeY - EYEBROW_OFFSET_Y;
  display.drawLine(leftEyeX - EYEBROW_HALF_WIDTH, eyebrowY, 
                   leftEyeX + EYEBROW_HALF_WIDTH, eyebrowY, 
                   SSD1306_WHITE);
  display.drawLine(rightEyeX - EYEBROW_HALF_WIDTH, eyebrowY, 
                   rightEyeX + EYEBROW_HALF_WIDTH, eyebrowY, 
                   SSD1306_WHITE);

  // Draw straight mouth (smaller than happy)
  int mouthWidth = 12;
  int mouthY = FACE_CENTER_Y + 12;
  display.drawLine(FACE_CENTER_X - mouthWidth/2, mouthY, 
                   FACE_CENTER_X + mouthWidth/2, mouthY, 
                   SSD1306_WHITE);

  display.display();
}

void drawEyebrow(int centerX, int centerY, bool leftSide) {
  int halfWidth = EYEBROW_HALF_WIDTH;
  int verticalTilt = EYEBROW_ANGLE;

  int x1 = centerX - halfWidth;
  int x2 = centerX + halfWidth;
  int y1 = centerY;
  int y2 = centerY;

  if (leftSide) {
    // For left eyebrow: outside end is higher (happier)
    y1 -= verticalTilt;
  } else {
    // For right eyebrow: outside end is higher (happier)
    y2 -= verticalTilt;
  }

  display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
} 