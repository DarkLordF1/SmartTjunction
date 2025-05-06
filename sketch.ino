// Smart T-Junction Project - traffic_control.ino
// Edwar Khoury - Spring 2025

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// LCD Pins
#define TFT_CS     22
#define TFT_RST    21
#define TFT_DC     20
#define TFT_MOSI   19
#define TFT_SCK    18
#define TFT_MISO   16
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Traffic Light Pins
const int TL1_R = 10, TL1_G = 9, TL1_B = 8;
const int TL2_R = 7,  TL2_G = 6, TL2_B = 5;
const int TL3_R = 4,  TL3_G = 3, TL3_B = 2;

// Button Pins
const int BTN1 = 14, BTN2 = 13, BTN3 = 12;
const int MODE_SWITCH = 11;

// Serial
#define BAUD_RATE 57600

// Constants
#define MAX_QUEUE 4
#define YELLOW_TIME 500
#define MIN_GREEN_TIME 1000
#define MAX_GREEN_TIME 7000

// Data Structures
struct Vehicle {
  unsigned long id;
  unsigned long arrivalTime;
};

struct Lane {
  Vehicle queue[MAX_QUEUE];
  int count = 0;
};

Lane lane1, lane2, lane3;
unsigned long vehicleCounter = 1;

// Round-robin and pattern
unsigned long lastOptimization = 0;
int greenPattern[3] = {3000, 3000, 3000};
int patternIndex = 0;
bool patternChanged = false;

// Helper Functions
void setTL(int tl, const char* color) {
  // Turn off all lights
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, HIGH); digitalWrite(TL1_B, HIGH);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, HIGH); digitalWrite(TL2_B, HIGH);
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, HIGH); digitalWrite(TL3_B, HIGH);

  int r, g, b;
  if (strcmp(color, "RED") == 0)    { r = LOW; g = HIGH; b = HIGH; }
  else if (strcmp(color, "GREEN") == 0)  { r = HIGH; g = LOW; b = HIGH; }
  else if (strcmp(color, "YELLOW") == 0) { r = HIGH; g = HIGH; b = LOW; }
  else return;

  switch (tl) {
    case 1: digitalWrite(TL1_R, r); digitalWrite(TL1_G, g); digitalWrite(TL1_B, b); break;
    case 2: digitalWrite(TL2_R, r); digitalWrite(TL2_G, g); digitalWrite(TL2_B, b); break;
    case 3: digitalWrite(TL3_R, r); digitalWrite(TL3_G, g); digitalWrite(TL3_B, b); break;
  }
  Serial.printf("%lu, system, TL%d set to %s\n", millis(), tl, color);
}

void logVehicleEvent(unsigned long id, const char* event) {
  Serial.printf("%lu, vehicle %lu, %s | Lane1:%d Lane2:%d Lane3:%d\n",
                millis(), id, event, lane1.count, lane2.count, lane3.count);
}

void generateVehicle(Lane &lane, int laneNum) {
  if (lane.count < MAX_QUEUE) {
    lane.queue[lane.count++] = {vehicleCounter++, millis()};
    logVehicleEvent(vehicleCounter - 1, "generated");
  } else {
    Serial.printf("%lu, vehicle -1, thrown from Lane %d (full)\n", millis(), laneNum);
  }
}

void crossVehicle(Lane &lane, int tlNum) {
  if (lane.count > 0) {
    unsigned long id = lane.queue[0].id;
    for (int i = 1; i < lane.count; ++i)
      lane.queue[i - 1] = lane.queue[i];
    lane.count--;
    logVehicleEvent(id, "crossed");
  }
}

void setup() {
  Serial.begin(BAUD_RATE);

  pinMode(TL1_R, OUTPUT); pinMode(TL1_G, OUTPUT); pinMode(TL1_B, OUTPUT);
  pinMode(TL2_R, OUTPUT); pinMode(TL2_G, OUTPUT); pinMode(TL2_B, OUTPUT);
  pinMode(TL3_R, OUTPUT); pinMode(TL3_G, OUTPUT); pinMode(TL3_B, OUTPUT);

  pinMode(BTN1, INPUT);
  pinMode(BTN2, INPUT);
  pinMode(BTN3, INPUT);
  pinMode(MODE_SWITCH, INPUT);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  Serial.println("System initialized");
}

void updateLCD() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.printf("Mode: %s\n", digitalRead(MODE_SWITCH) ? "Manual" : "Random");
  tft.printf("TL1:%d TL2:%d TL3:%d\n", lane1.count, lane2.count, lane3.count);
  tft.printf("Pattern: %d-%d-%d\n", greenPattern[0]/1000, greenPattern[1]/1000, greenPattern[2]/1000);
}

void loop() {
  updateLCD();

  if (digitalRead(MODE_SWITCH)) { // Manual mode
    if (digitalRead(BTN1)) generateVehicle(lane1, 1);
    if (digitalRead(BTN2)) generateVehicle(lane2, 2);
    if (digitalRead(BTN3)) generateVehicle(lane3, 3);
  } else {
    // TODO: Implement random Markov process vehicle generation
  }

  // Traffic Light Scheduler
  int tl = patternIndex + 1;
  setTL(tl, "GREEN");
  delay(greenPattern[patternIndex]);

  if (tl == 1) crossVehicle(lane1, tl);
  else if (tl == 2) crossVehicle(lane2, tl);
  else if (tl == 3) crossVehicle(lane3, tl);

  setTL(tl, "YELLOW");
  delay(YELLOW_TIME);
  setTL(tl, "RED");

  patternIndex = (patternIndex + 1) % 3;

  // Every 30 seconds optimize pattern (placeholder)
  if (millis() - lastOptimization >= 30000) {
    Serial.printf("%lu, system, Traffic Light Pattern Optimization\n", millis());
    // TODO: Implement optimization logic based on dropped cars, queue stats, etc.
    lastOptimization = millis();
  }
}
