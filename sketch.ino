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
#define YELLOW_TIME 5000
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
  bool markovState = true;
  unsigned long nextGenTime = 0;
};

Lane lane1, lane2, lane3;
unsigned long vehicleCounter = 1;

unsigned long lastOptimization = 0;
int greenPattern[3] = {3000, 3000, 3000};
int patternIndex = 0;

int thrownCars[3] = {0, 0, 0};
const char* tlColors[3] = {"RED", "RED", "RED"};

void setTL(const char* color, bool isTL3Phase = false) {
  int rA = LOW, gA = LOW, bA = LOW; // TL1 & TL2
  int rB = LOW, gB = LOW, bB = LOW; // TL3

  if (isTL3Phase) {
    // TL3 gets active color, TL1 & TL2 stay red
    rA = HIGH;

    if (strcmp(color, "GREEN") == 0) {
      gB = HIGH;
    } else if (strcmp(color, "YELLOW") == 0) {
      rB = gB = HIGH;
    } else {
      rB = HIGH;
    }
  } else {
    // TL1 & TL2 get active color, TL3 stays red
    rB = HIGH;

    if (strcmp(color, "GREEN") == 0) {
      gA = HIGH;
    } else if (strcmp(color, "YELLOW") == 0) {
      rA = gA = HIGH;
    } else {
      rA = HIGH;
    }
  }

  // Update internal status
  tlColors[0] = tlColors[1] = (gA && rA) ? "YELLOW" : (gA ? "GREEN" : "RED");
  tlColors[2] = (gB && rB) ? "YELLOW" : (gB ? "GREEN" : "RED");

  digitalWrite(TL1_R, rA); digitalWrite(TL1_G, gA); digitalWrite(TL1_B, bA);
  digitalWrite(TL2_R, rA); digitalWrite(TL2_G, gA); digitalWrite(TL2_B, bA);
  digitalWrite(TL3_R, rB); digitalWrite(TL3_G, gB); digitalWrite(TL3_B, bB);

  Serial.printf("%lu, system, TL1 set to %s | TL2 set to %s | TL3 set to %s\n",
                millis(), tlColors[0], tlColors[1], tlColors[2]);
}



void logVehicleEvent(unsigned long id, const char* event) {
  Serial.printf("%lu, vehicle %lu, %s | Lane1:%d Lane2:%d Lane3:%d\n",
                millis(), id, event, lane1.count, lane2.count, lane3.count);
}

void generateVehicle(Lane &lane, int laneNum) {
  if (digitalRead(MODE_SWITCH) == HIGH) return; // block generation in manual mode
  if (lane.count < MAX_QUEUE) {
    lane.queue[lane.count++] = {vehicleCounter++, millis()};
    logVehicleEvent(vehicleCounter - 1, "generated");
  } else {
    thrownCars[laneNum - 1]++;
    Serial.printf("%lu, vehicle -1, thrown from Lane %d (full)\n", millis(), laneNum);
  }
}

void crossVehicle(Lane &lane, int tlNum, int maxPass) {
  for (int i = 0; i < maxPass && lane.count > 0; i++) {
    unsigned long id = lane.queue[0].id;
    for (int j = 1; j < lane.count; ++j)
      lane.queue[j - 1] = lane.queue[j];
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
  static char lastDisplay[256];
  char buffer[256];

  snprintf(buffer, sizeof(buffer),
  "Mode: %s\n"
  "TL #1: %s\n"
  "TL #2: %s\n"
  "TL #3: %s\n"

  "L1:%d L2:%d L3:%d\n"
  "Green(s): %d %d %d\n"
  "Thrown: %d\n",
  digitalRead(MODE_SWITCH) ? "Manual" : "Random",
  tlColors[0], tlColors[1], tlColors[2],
  lane1.count, lane2.count, lane3.count,
  greenPattern[0] / 1000, greenPattern[1] / 1000, greenPattern[2] / 1000,
  thrownCars[0] + thrownCars[1] + thrownCars[2]
);


  if (strcmp(buffer, lastDisplay) != 0) {
    strcpy(lastDisplay, buffer);
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.print(buffer);
  }
}

void updateMarkov(Lane &lane, int laneNum) {
  if (millis() >= lane.nextGenTime) {
    if (lane.markovState) {
      generateVehicle(lane, laneNum);
      lane.nextGenTime = millis() + 5000;
      lane.markovState = random(100) < 70;
    } else {
      int w = random(500, 5001);
      lane.nextGenTime = millis() + w;
      lane.markovState = random(100) >= 20;
    }
  }
}

void optimizePattern() {
  Serial.printf("%lu, system, Traffic Light Pattern Optimization\n", millis());
  int totalThrown = thrownCars[0] + thrownCars[1] + thrownCars[2];
  if (totalThrown == 0) return;
  for (int i = 0; i < 3; i++)
    greenPattern[i] = map(thrownCars[i], 0, totalThrown, MIN_GREEN_TIME, MAX_GREEN_TIME);
  Serial.printf("%lu, system, Traffic Light Pattern Update\n", millis());
  for (int i = 0; i < 3; i++) thrownCars[i] = 0;
}

void loop() {
  updateLCD();

  if (digitalRead(MODE_SWITCH)) {
    if (digitalRead(BTN1)) generateVehicle(lane1, 1);
    if (digitalRead(BTN2)) generateVehicle(lane2, 2);
    if (digitalRead(BTN3)) generateVehicle(lane3, 3);
  } else {
    updateMarkov(lane1, 1);
    updateMarkov(lane2, 2);
    updateMarkov(lane3, 3);
  }

bool tl3Phase = (patternIndex % 2 == 1);  // every other pattern

if (tl3Phase) {
  // TL3 gets green, TL1 & TL2 red
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  digitalWrite(TL3_R, LOW);  digitalWrite(TL3_G, HIGH);
  tlColors[0] = tlColors[1] = "RED";
  tlColors[2] = "GREEN";
} else {
  setTL("GREEN");  // TL1 & TL2 green, TL3 red
}

unsigned long greenStart = millis();


  while (millis() - greenStart < greenPattern[patternIndex]) {
    static unsigned long lastLCD = 0;
    if (millis() - lastLCD >= 1000) { // update LCD once every second
      updateLCD();
      lastLCD = millis();
    }
  }


if (tl3Phase) {
  crossVehicle(lane3, 3, 2);
} else {
  crossVehicle(lane1, 1, 2);
  crossVehicle(lane2, 2, 2);
}


if (tl3Phase) {
  // TL3 gets yellow
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, HIGH);
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  tlColors[2] = "YELLOW";
  tlColors[0] = tlColors[1] = "RED";
} else {
  setTL("YELLOW"); // TL1 & TL2 yellow, TL3 red
}

  delay(YELLOW_TIME);
  setTL("RED");

  patternIndex = (patternIndex + 1) % 3;
  if (millis() - lastOptimization >= 30000) {
    optimizePattern();
    lastOptimization = millis();
  }
}
