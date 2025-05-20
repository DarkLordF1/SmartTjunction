#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <SD.h>

// === Graphics constants for on‐LCD traffic lights ===
const int lightX[3] = {50, 100, 150};
const int lightY    = 200;
const int rectW     = 30;
const int rectH     = 70;
const int r         = 8;    // circle radius

// === LCD and Serial Configuration ===
#define TFT_CS     22
#define TFT_RST    21
#define TFT_DC     20
#define TFT_MOSI   19
#define TFT_SCK    18
#define TFT_MISO   16
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

#define BAUD_RATE       57600
#define MAX_QUEUE       4
#define YELLOW_TIME     500    // 0.5s
#define MIN_GREEN_TIME  1000   // 1s
#define MAX_GREEN_TIME  7000   // 7s

// === Traffic Light pins (R, G, B cathodes) ===
const int TL1_R = 10, TL1_G = 9,  TL1_B = 8;
const int TL2_R = 7,  TL2_G = 6,  TL2_B = 5;
const int TL3_R = 4,  TL3_G = 3,  TL3_B = 2;

// === Buttons & Mode Switch ===
const int BTN1 = 14, BTN2 = 13, BTN3 = 12;
const int MODE_SWITCH = 11;
const unsigned long PRESS_COOLDOWN = 250;

// === Vehicle & Lane Structures ===
struct Vehicle { unsigned long id, arrivalTime; };
struct Lane {
  Vehicle queue[MAX_QUEUE];
  int     count       = 0;
  bool    markovState = true;
  unsigned long nextGenTime = 0;
};
Lane lane1, lane2, lane3;

unsigned long vehicleCounter = 1;
int           thrownCars[3]  = {0,0,0};

// === Phase & Pattern ===
int greenPattern[3]    = {3000,3000,3000};
int patternIndex       = 0;
unsigned long lastOpt  = 0;
unsigned long lastCross= 0;

// === TL state for LCD icons ===
enum TLState { S_RED, S_YELLOW, S_GREEN };
TLState tlState[3] = { S_RED, S_RED, S_RED };

// ——— SD card logging —————————————————————————————————————————————————————
#define SD_CS       5
#define LOG_NAME    "my junction.log"
File logFile;

void initLogging() {
  Serial1.begin(BAUD_RATE);
  if (SD.begin(SD_CS)) {
    logFile = SD.open(LOG_NAME, FILE_WRITE);
    if (!logFile) {
      Serial1.println("SD: failed to open log file");
    }
  } else {
    Serial1.println("SD: init failed");
  }
}

// timestamp, system, event, TLx old->new
void logTLChange(int tlIndex, TLState oldState, TLState newState) {
  static const char* names[] = {"RED","YELLOW","GREEN"};
  String evt = String("Traffic Light Changing Color: TL") +
               String(tlIndex+1) + " " +
               names[oldState] + "→" + names[newState];
  String line = String(millis()) + ", system, " + evt;
  Serial1.println(line);
  if (logFile) { logFile.println(line); logFile.flush(); }
}

// timestamp, system, event
void logSystemEvent(const char* event) {
  String line = String(millis()) + ", system, " + event;
  Serial1.println(line);
  if (logFile) { logFile.println(line); logFile.flush(); }
}

// timestamp, id, event, entered/exited lane X, Lane statuses...
void logVehicleEvent(unsigned long id, const char* event, int laneNum, bool isEntry) {
  String action = isEntry ? "entered" : "crossed";
  String status = "Lane 1: " + String(lane1.count)
                + ", Lane 2: " + String(lane2.count)
                + ", Lane 3: " + String(lane3.count);
  String line = String(millis())
              + ", " + String(id)
              + ", " + event
              + ", " + action + " lane " + String(laneNum)
              + ", " + status;
  Serial1.println(line);
  if (logFile) { logFile.println(line); logFile.flush(); }
}

// ——— Hardware Helpers —————————————————————————————————————————————————

void setGreen(int p) {
  // turn off all blue channels
  digitalWrite(TL1_B, LOW);
  digitalWrite(TL2_B, LOW);
  digitalWrite(TL3_B, LOW);

  // all red first
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, LOW);

  TLState old = tlState[p];
  // then that one green
  if (p==0) { digitalWrite(TL1_R, LOW);  digitalWrite(TL1_G, HIGH); }
  if (p==1) { digitalWrite(TL2_R, LOW);  digitalWrite(TL2_G, HIGH); }
  if (p==2) { digitalWrite(TL3_R, LOW);  digitalWrite(TL3_G, HIGH); }

  for (int i=0; i<3; i++)
    tlState[i] = (i==p ? S_GREEN : S_RED);

  logTLChange(p, old, S_GREEN);
}

void setYellow(int p) {
  // turn off all blue channels
  digitalWrite(TL1_B, LOW);
  digitalWrite(TL2_B, LOW);
  digitalWrite(TL3_B, LOW);

  // all red first
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, LOW);

  TLState old = tlState[p];
  // then that one yellow
  if (p==0) { digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, HIGH); }
  if (p==1) { digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, HIGH); }
  if (p==2) { digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, HIGH); }

  for (int i=0; i<3; i++)
    tlState[i] = (i==p ? S_YELLOW : S_RED);

  logTLChange(p, old, S_YELLOW);
}

void crossOne(int p) {
  Lane &L = (p==0 ? lane1 : p==1 ? lane2 : lane3);
  if (L.count > 0) {
    unsigned long vid = L.queue[0].id;
    for (int i=1; i<L.count; i++) L.queue[i-1] = L.queue[i];
    L.count--;
    logVehicleEvent(vid, "Junction Crossing", p+1, false);
  }
}

void generateVehicles() {
  static int prevB[3] = {HIGH,HIGH,HIGH};
  static unsigned long tmr[3] = {0,0,0};
  unsigned long now = millis();
  int b[3] = { digitalRead(BTN1), digitalRead(BTN2), digitalRead(BTN3) };

  if (digitalRead(MODE_SWITCH)==HIGH) {
    for (int i=0; i<3; i++) {
      if (b[i]==LOW && prevB[i]==HIGH && now-tmr[i]>PRESS_COOLDOWN) {
        Lane &L = (i==0?lane1:i==1?lane2:lane3);
        if (L.count < MAX_QUEUE) {
          unsigned long vid = vehicleCounter++;
          L.queue[L.count++] = {vid, now};
          logVehicleEvent(vid, "Vehicle Generation", i+1, true);
        } else {
          thrownCars[i]++;
        }
        tmr[i] = now;
      }
      prevB[i] = b[i];
    }
  } else {
    for (int i=0; i<3; i++) {
      Lane &L = (i==0?lane1:i==1?lane2:lane3);
      if (now >= L.nextGenTime) {
        if (L.markovState) {
          if (L.count < MAX_QUEUE) {
            unsigned long vid = vehicleCounter++;
            L.queue[L.count++] = {vid, now};
            logVehicleEvent(vid, "Vehicle Generation (random)", i+1, true);
          } else {
            thrownCars[i]++;
          }
          L.nextGenTime = now + 5000;
          L.markovState = false;
        } else {
          L.nextGenTime = now + random(500,5000);
          L.markovState = true;
        }
      }
    }
  }
}

void optimizePattern() {
  int sum = thrownCars[0] + thrownCars[1] + thrownCars[2];
  if (!sum) return;
  for (int i=0; i<3; i++) {
    greenPattern[i] = map(thrownCars[i], 0, sum, MIN_GREEN_TIME, MAX_GREEN_TIME);
    thrownCars[i] = 0;
  }
  logSystemEvent("Traffic Light Pattern Optimization");
}

// ——— LCD UI ————————————————————————————————————————————————————

void drawStaticUI() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(0,0);   tft.print("MODE:");
  tft.setCursor(0,20);  tft.print("TL#1:");
  tft.setCursor(0,40);  tft.print("TL#2:");
  tft.setCursor(0,60);  tft.print("TL#3:");
  tft.setCursor(0,80);  tft.print("CARS:");
  tft.setCursor(0,100); tft.print("GREEN:");
  tft.setCursor(0,120); tft.print("THROWN:");
  for (int i=0; i<3; i++){
    int x = lightX[i], y = lightY, dy = rectH/2 - r - 2;
    tft.drawRect(x-rectW/2, y-rectH/2, rectW, rectH, ILI9341_WHITE);
    tft.drawCircle(x, y-dy, r, ILI9341_WHITE);
    tft.drawCircle(x, y,    r, ILI9341_WHITE);
    tft.drawCircle(x, y+dy, r, ILI9341_WHITE);
  }
}

void updateLCD() {
  static String lastMode   = "";
  static String lastCars   = "";
  static String lastGreen  = "";
  static String lastThrown = "";
  static String lastTxt[3] = {"","",""};
  static TLState lastSt[3] = { (TLState)-1, (TLState)-1, (TLState)-1 };

  // 1) MODE
  String m = digitalRead(MODE_SWITCH) ? "Manual" : "Random";
  if (m != lastMode) {
    tft.fillRect(80,0,160,16,ILI9341_BLACK);
    tft.setCursor(80,0); tft.print(m);
    lastMode = m;
  }

  // 2) TL Text + Icons
  const int yTL[3] = {20,40,60};
  for (int i=0; i<3; i++){
    TLState st = tlState[i];
    String txt = (st==S_GREEN)    ? "GREEN"
                 : (st==S_YELLOW) ? "YELLOW"
                                  : "RED";
    if (txt != lastTxt[i]) {
      tft.fillRect(80,yTL[i],160,16,ILI9341_BLACK);
      uint16_t col = (st==S_GREEN)    ? ILI9341_GREEN
                      : (st==S_YELLOW)? ILI9341_YELLOW
                                      : ILI9341_RED;
      tft.setTextColor(col,ILI9341_BLACK);
      tft.setCursor(80,yTL[i]); tft.print(txt);
      tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);
      lastTxt[i] = txt;
    }
    if (st != lastSt[i]) {
      int x = lightX[i], y = lightY, dy = rectH/2 - r - 2;
      int yPos[3] = { y + dy, y, y - dy }; // Green top, Yellow mid, Red bottom
      for(int j=0;j<3;j++){
        tft.fillCircle(x,yPos[j],r,ILI9341_BLACK);
        tft.drawCircle(x,yPos[j],r,ILI9341_WHITE);
      }
      int idx = (st==S_GREEN)?0:(st==S_YELLOW)?1:2;
      uint16_t col = (st==S_GREEN)?ILI9341_GREEN:(st==S_YELLOW)?ILI9341_YELLOW:ILI9341_RED;
      tft.fillCircle(x,yPos[idx],r,col);
      tft.drawCircle(x,yPos[idx],r,ILI9341_WHITE);
      lastSt[i] = st;
    }
  }

  // 3) CARS
  String cars = String(lane1.count)+","+lane2.count+","+lane3.count;
  if (cars != lastCars){
    tft.fillRect(80,80,160,16,ILI9341_BLACK);
    tft.setCursor(80,80);
    tft.print("L1:"+String(lane1.count)+" L2:"+String(lane2.count)+" L3:"+String(lane3.count));
    lastCars = cars;
  }

  // 4) GREEN times
  String gr = String(greenPattern[0]/1000)+","+String(greenPattern[1]/1000)+","+String(greenPattern[2]/1000);
  if (gr != lastGreen){
    tft.fillRect(80,100,160,16,ILI9341_BLACK);
    tft.setCursor(80,100); tft.print(gr);
    lastGreen = gr;
  }

  // 5) THROWN
  int tot = thrownCars[0]+thrownCars[1]+thrownCars[2];
  String th = String(tot);
  if (th != lastThrown){
    tft.fillRect(80,120,160,16,ILI9341_BLACK);
    tft.setCursor(80,120); tft.print(th);
    lastThrown = th;
  }
}

void setup(){
  initLogging();
  pinMode(TL1_R,OUTPUT); pinMode(TL1_G,OUTPUT); pinMode(TL1_B,OUTPUT);
  pinMode(TL2_R,OUTPUT); pinMode(TL2_G,OUTPUT); pinMode(TL2_B,OUTPUT);
  pinMode(TL3_R,OUTPUT); pinMode(TL3_G,OUTPUT); pinMode(TL3_B,OUTPUT);
  pinMode(BTN1,INPUT_PULLUP); pinMode(BTN2,INPUT_PULLUP);
  pinMode(BTN3,INPUT_PULLUP); pinMode(MODE_SWITCH,INPUT);
  tft.begin(); tft.setRotation(3);
  drawStaticUI();
  logSystemEvent("Traffic Light Pattern Update");
}

void loop(){
  unsigned long now = millis();
  generateVehicles();
  updateLCD();
  int p = patternIndex;
  lastCross = now;

  // GREEN PHASE
  setGreen(p);
  unsigned long start = now;
  while (millis() - start < greenPattern[p]){
    unsigned long cur = millis();
    if (cur - lastCross >= 1000){
      crossOne(p);
      lastCross = cur;
    }
    generateVehicles();
    updateLCD();
  }

  // YELLOW PHASE
  setYellow(p);
  start = millis();
  while (millis() - start < YELLOW_TIME){
    generateVehicles();
    updateLCD();
  }

  // final cross & advance
  crossOne(p);
  patternIndex = (p+1) % 3;
  if (now - lastOpt >= 30000){
    optimizePattern();
    lastOpt = now;
  }
}
