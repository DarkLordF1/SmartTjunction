#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>


// === Graphics constants for on-LCD traffic lights ===
const int lightX[3] = {50, 100, 150};
const int lightY    = 200;
const int rectW     = 30;
const int rectH     = 70;
const int r         = 8;    // circle radius

// mirror of each TL's state for LCD
enum TLState { S_RED, S_YELLOW, S_GREEN };
TLState tlState[3];

// === LCD and Serial Configuration ===
#define TFT_CS     22
#define TFT_RST    21
#define TFT_DC     20
#define TFT_MOSI   19
#define TFT_SCK    18
#define TFT_MISO   16
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
#define BAUD_RATE 57600

// === Traffic Light Pin Assignments ===
const int TL1_R = 10, TL1_G = 9;
const int TL2_R = 7,  TL2_G = 6;
const int TL3_R = 4,  TL3_G = 3;

// === Button & Mode Switch ===
const int BTN1 = 14, BTN2 = 13, BTN3 = 12;
const int MODE_SWITCH = 11;
const unsigned long PRESS_COOLDOWN = 250;

// === Timing Constants ===
#define MAX_QUEUE      4
#define YELLOW_TIME    500    // 0.5s
#define MIN_GREEN_TIME 1000   // 1s
#define MAX_GREEN_TIME 7000   // 7s

// === Vehicle & Lane ===
struct Vehicle { unsigned long id, arrivalTime; };
struct Lane {
  Vehicle queue[MAX_QUEUE];
  int     count       = 0;
  bool    markovState = true;
  unsigned long nextGenTime = 0;
};
Lane lane1, lane2, lane3;
unsigned long vehicleCounter  = 1;
int           thrownCars[3]   = {0,0,0};

// === Pattern & Phases ===
int greenPattern[3] = {3000, 3000, 3000};  // ms for TL1, TL2, TL3
int patternIndex    = 0;
unsigned long lastOptimization = 0;
unsigned long lastDecay        = 0;

// For LCD tracking
const char* tlColors[3] = {"RED","RED","RED"};


// ——— Helpers —————————————————————————————————————————————————————

// Turn exactly TL#p green, others red
void setGreen(int p) {
  // All red first
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, LOW);
  // That one green
  if (p==0) digitalWrite(TL1_R, LOW), digitalWrite(TL1_G, HIGH);
  if (p==1) digitalWrite(TL2_R, LOW), digitalWrite(TL2_G, HIGH);
  if (p==2) digitalWrite(TL3_R, LOW), digitalWrite(TL3_G, HIGH);
  for(int i=0;i<3;i++) tlColors[i] = (i==p?"GREEN":"RED");
  Serial1.printf("%lu, system, TL1=%s | TL2=%s | TL3=%s\n",
    millis(), tlColors[0], tlColors[1], tlColors[2]);
}

// Turn exactly TL#p yellow, others red
void setYellow(int p) {
  digitalWrite(TL1_R, HIGH); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, HIGH); digitalWrite(TL2_G, LOW);
  digitalWrite(TL3_R, HIGH); digitalWrite(TL3_G, LOW);
  if (p==0) digitalWrite(TL1_R, HIGH), digitalWrite(TL1_G, HIGH);
  if (p==1) digitalWrite(TL2_R, HIGH), digitalWrite(TL2_G, HIGH);
  if (p==2) digitalWrite(TL3_R, HIGH), digitalWrite(TL3_G, HIGH);
  for(int i=0;i<3;i++) tlColors[i] = (i==p?"YELLOW":"RED");
  Serial1.printf("%lu, system, TL#%d YELLOW\n", millis(), p+1);
}

// Cross exactly one car from lane p
void crossOne(int p) {
  Lane &L = (p==0?lane1: p==1?lane2: lane3);
  if (L.count>0) {
    for(int i=1;i<L.count;i++) L.queue[i-1]=L.queue[i];
    L.count--;
    Serial1.printf("%lu, system, crossed from Lane%d\n", millis(), p+1);
  }
}

// Generate vehicles: manual buttons + random Markov
void generateVehicles() {
  // Manual
  static bool prev1=HIGH, prev2=HIGH, prev3=HIGH;
  static unsigned long t1=0,t2=0,t3=0;
  unsigned long now=millis();
  bool b1=digitalRead(BTN1),
       b2=digitalRead(BTN2),
       b3=digitalRead(BTN3);
  if (digitalRead(MODE_SWITCH)==HIGH) {
    if(!b1&&prev1&&now-t1>PRESS_COOLDOWN) lane1.queue[lane1.count++]={vehicleCounter++,now}, t1=now;
    if(!b2&&prev2&&now-t2>PRESS_COOLDOWN) lane2.queue[lane2.count++]={vehicleCounter++,now}, t2=now;
    if(!b3&&prev3&&now-t3>PRESS_COOLDOWN) lane3.queue[lane3.count++]={vehicleCounter++,now}, t3=now;
  }
  prev1=b1; prev2=b2; prev3=b3;
  // Random
  if(digitalRead(MODE_SWITCH)==LOW){
    for(int i=0;i<3;i++){
      Lane &L = (i==0?lane1: i==1?lane2: lane3);
      if(millis()>=L.nextGenTime){
        if(L.markovState){
          if(L.count<MAX_QUEUE) L.queue[L.count++]={vehicleCounter++,millis()};
          else thrownCars[i]++;
          L.nextGenTime=millis()+5000;
          L.markovState=false;
        } else {
          L.nextGenTime=millis()+random(500,5000);
          L.markovState=true;
        }
      }
    }
  }
}

// Optimize pattern every 30s at cycle end
void optimizePattern() {
  int sum=thrownCars[0]+thrownCars[1]+thrownCars[2];
  if(!sum) return;
  for(int i=0;i<3;i++){
    greenPattern[i] = map(thrownCars[i],0,sum,MIN_GREEN_TIME,MAX_GREEN_TIME);
    thrownCars[i] = 0;
  }
  Serial1.printf("%lu, system, new pattern %d,%d,%d\n",
    millis(), greenPattern[0],greenPattern[1],greenPattern[2]);
}


// === LCD UI ————————————————————————————————————————————————————

void drawStaticUI(){
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

  // draw three TL boxes + outlines
  for (int i=0; i<3; i++){
    int x = lightX[i], y = lightY;
    tft.drawRect(x - rectW/2, y - rectH/2, rectW, rectH, ILI9341_WHITE);
    int dy = rectH/2 - r - 2;
    tft.drawCircle(x, y - dy, r, ILI9341_WHITE);
    tft.drawCircle(x, y    , r, ILI9341_WHITE);
    tft.drawCircle(x, y + dy, r, ILI9341_WHITE);
  }
}

void updateLCD() {
  // --- keep track of last values ---
  static String lastMode     = "";
  static String lastTLText[3] = {"", "", ""};
  static String lastCars     = "";
  static String lastGreen    = "";
  static String lastThrown   = "";
  // last TL icon state
  static TLState lastState[3] = { S_RED, S_RED, S_RED };

  // --- 1) MODE ---
  String mode = digitalRead(MODE_SWITCH) ? "Manual" : "Random";
  if (mode != lastMode) {
    tft.fillRect(80, 0, 160, 16, ILI9341_BLACK);
    tft.setCursor(80, 0);
    tft.print(mode);
    lastMode = mode;
  }

  // --- 2) TL TEXT & ICONS ---
  const int yTL[3] = { 20, 40, 60 };
  for (int i = 0; i < 3; i++) {
    // current state
    TLState st = tlState[i];
    String txt;
    uint16_t col;
    if (st == S_GREEN)  { txt = "GREEN";  col = ILI9341_GREEN;  }
    else if (st == S_YELLOW) { txt = "YELLOW"; col = ILI9341_YELLOW; }
    else                { txt = "RED";    col = ILI9341_RED;    }

    // 2a) update text if changed
    if (txt != lastTLText[i]) {
      tft.fillRect(80, yTL[i], 160, 16, ILI9341_BLACK);
      tft.setTextColor(col, ILI9341_BLACK);
      tft.setCursor(80, yTL[i]);
      tft.print(txt);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      lastTLText[i] = txt;
    }

    // 2b) update icon circle
    if (st != lastState[i]) {
      int x = lightX[i];
      int y = lightY;
      int dy = rectH/2 - r - 2;
      int yG = y - dy;
      int yY = y;
      int yR = y + dy;

      // clear previous
      switch (lastState[i]) {
        case S_GREEN:  tft.fillCircle(x, yG, r, ILI9341_BLACK); break;
        case S_YELLOW: tft.fillCircle(x, yY, r, ILI9341_BLACK); break;
        case S_RED:    tft.fillCircle(x, yR, r, ILI9341_BLACK); break;
      }

      // draw new
      if (st == S_GREEN)      tft.fillCircle(x, yG, r, col);
      else if (st == S_YELLOW) tft.fillCircle(x, yY, r, col);
      else                     tft.fillCircle(x, yR, r, col);

      lastState[i] = st;
    }
  }

  // --- 3) CARS ---
  String cars = String(lane1.count) + "," +
                String(lane2.count) + "," +
                String(lane3.count);
  if (cars != lastCars) {
    tft.fillRect(80, 80, 160, 16, ILI9341_BLACK);
    tft.setCursor(80, 80);
    tft.print("L1:" + String(lane1.count) +
              " L2:" + String(lane2.count) +
              " L3:" + String(lane3.count));
    lastCars = cars;
  }

  // --- 4) GREEN TIMES ---
  String gr = String(greenPattern[0] / 1000) + "," +
              String(greenPattern[1] / 1000) + "," +
              String(greenPattern[2] / 1000);
  if (gr != lastGreen) {
    tft.fillRect(80, 100, 160, 16, ILI9341_BLACK);
    tft.setCursor(80, 100);
    tft.print(gr);
    lastGreen = gr;
  }

  // --- 5) THROWN ---
  String th = String(thrownCars[0] + thrownCars[1] + thrownCars[2]);
  if (th != lastThrown) {
    tft.fillRect(80, 120, 160, 16, ILI9341_BLACK);
    tft.setCursor(80, 120);
    tft.print(th);
    lastThrown = th;
  }
}


// === Setup & Loop ——————————————————————————————————————————————

void setup(){
  Serial1.begin(BAUD_RATE);
  pinMode(TL1_R,OUTPUT); pinMode(TL1_G,OUTPUT);
  pinMode(TL2_R,OUTPUT); pinMode(TL2_G,OUTPUT);
  pinMode(TL3_R,OUTPUT); pinMode(TL3_G,OUTPUT);
  pinMode(BTN1,INPUT_PULLUP); pinMode(BTN2,INPUT_PULLUP); pinMode(BTN3,INPUT_PULLUP);
  pinMode(MODE_SWITCH,INPUT);
  tft.begin(); tft.setRotation(3);
  drawStaticUI();
  Serial1.println("System initialized");
}

void loop(){
  unsigned long now=millis();
  // 1‐car/sec decay
  if(now-lastDecay>=1000){
    if(lane1.count>0) lane1.count--;
    if(lane2.count>0) lane2.count--;
    if(lane3.count>0) lane3.count--;
    lastDecay=now;
  }
  // vehicle gen + lcd
  generateVehicles();
  updateLCD();
  // run phase p
  int p = patternIndex;
  unsigned long start = now;
  // green (minus yellow)
  setGreen(p);
  while(millis()-start < greenPattern[p]-YELLOW_TIME){
    generateVehicles(); updateLCD();
  }
  // yellow
  setYellow(p);
  start=millis();
  while(millis()-start < YELLOW_TIME){
    generateVehicles(); updateLCD();
  }
  // cross one
  crossOne(p);
  // next phase & optimize
  patternIndex=(patternIndex+1)%3;
  if(now-lastOptimization>=30000){
    optimizePattern();
    lastOptimization=now;
  }
}
