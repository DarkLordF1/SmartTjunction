#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// --- TFT Display Pins ---------------------------------------------------
#define TFT_CS     22
#define TFT_RST    21
#define TFT_DC     20
#define TFT_MOSI   19
#define TFT_SCK    18
#define TFT_MISO   16
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// --- Traffic Light Pins -------------------------------------------------
const int TL1_R = 10, TL1_G = 9,  TL1_B = 8;
const int TL2_R = 7,  TL2_G = 6,  TL2_B = 5;
const int TL3_R = 4,  TL3_G = 3,  TL3_B = 2;
static const int TL_PINS[3][3] = {
  {TL1_R, TL1_G, TL1_B},
  {TL2_R, TL2_G, TL2_B},
  {TL3_R, TL3_G, TL3_B}
};

// --- Buttons & Mode ------------------------------------------------------
const int BTN1 = 14, BTN2 = 13, BTN3 = 12;
const int MODE_SWITCH = 11;  // when HIGH = manual mode

// --- Serial & Timing -----------------------------------------------------
#define BAUD_RATE        57600
#define MAX_QUEUE        4
#define YELLOW_TIME      500    // ms
#define MIN_GREEN_TIME   1000   // ms
#define MAX_GREEN_TIME   7000   // ms
#define OPTIM_INTERVAL   30000  // ms
#define DISPLAY_INTERVAL 100    // ms

// --- Debounce ------------------------------------------------------------
#define DEBOUNCE_DELAY   50     // ms
static unsigned long lastPressTime[3] = {0,0,0};
static bool prevBtn[3] = {false,false,false};

// --- Markov Arrival Params -----------------------------------------------
#define MARKOV_BASE_INTERVAL 5000  // ms
#define MARKOV_P_GREEN        70   // % chance to continue arrivals
#define MARKOV_P_OFF          20   // % chance to restart arrivals

// --- Enums & Globals -----------------------------------------------------
enum LightColor { RED = 0, GREEN = 1, YELLOW = 2 };
static LightColor currentTL[3] = { RED, RED, RED };
int greenPattern[3] = { 3000, 3000, 3000 };

// --- Vehicle & Lane Structures ------------------------------------------
struct Vehicle { unsigned long id, arrivalTime; };
struct Lane {
  Vehicle q[MAX_QUEUE];        // vehicle queue
  int count = 0;               // current queue length
  int dropped = 0;             // dropped when full
  bool markovState = true;     // Markov on/off state
  unsigned long nextGenTime;   // next generate time
};
Lane lane1, lane2, lane3;
unsigned long vehicleCounter = 1;

// --- State ---------------------------------------------------------------
unsigned long lastOptimization = 0;
unsigned long lastDisplay = 0;
int lastLane = 1;

// --- Function Prototypes -----------------------------------------------
void generateVehicle(Lane &ln, int num);
void updateMarkov(Lane &ln, int num);
void optimizePattern();
void setTL(int tl, LightColor color);
void crossVehicle(Lane &ln, int maxPass);
void updateLCD();
const char* colorToString(LightColor c);

// --- Setup ---------------------------------------------------------------
void setup() {
  Serial.begin(BAUD_RATE);
  randomSeed(analogRead(A0));           // RNG seed

  // Init TFT
  SPI.begin();
  tft.begin(42000000UL);
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  // Traffic-light pins
  for (int i = 0; i < 3; i++) {
    pinMode(TL_PINS[i][0], OUTPUT);
    pinMode(TL_PINS[i][1], OUTPUT);
    pinMode(TL_PINS[i][2], OUTPUT);
    digitalWrite(TL_PINS[i][0], LOW);
    digitalWrite(TL_PINS[i][1], LOW);
    digitalWrite(TL_PINS[i][2], LOW);
  }

  // Buttons & mode switch
  pinMode(BTN1, INPUT_PULLDOWN);
  pinMode(BTN2, INPUT_PULLDOWN);
  pinMode(BTN3, INPUT_PULLDOWN);
  pinMode(MODE_SWITCH, INPUT_PULLDOWN);

  // Initialize Markov timers
  unsigned long now = millis();
  lane1.nextGenTime = lane2.nextGenTime = lane3.nextGenTime = now;
}

// --- Main Loop -----------------------------------------------------------
void loop() {
  unsigned long now = millis();
  bool btnState[3] = { digitalRead(BTN1), digitalRead(BTN2), digitalRead(BTN3) };
  bool manual = digitalRead(MODE_SWITCH);

  // Manual generation (always check buttons), with debounce
  for (int i = 0; i < 3; i++) {
    if (btnState[i] && !prevBtn[i] && (now - lastPressTime[i] > DEBOUNCE_DELAY)) {
      // enqueue car in lane i+1
      generateVehicle(i==0?lane1:(i==1?lane2:lane3), i+1);
      lastPressTime[i] = now;
    }
    prevBtn[i] = btnState[i];
  }

  // Markov-driven arrivals when NOT in manual mode
  if (!manual) {
    updateMarkov(lane1, 1);
    updateMarkov(lane2, 2);
    updateMarkov(lane3, 3);
  }

  // 30s optimization
  if (now - lastOptimization >= OPTIM_INTERVAL) {
    optimizePattern();
    lastOptimization = now;
  }

  // Traffic-light state machine (non-blocking)
  static unsigned long phaseStart = 0;
  static int phase = 0, currentLane = 1;
  if (phaseStart == 0) {
    phaseStart = now;
    setTL(currentLane, GREEN);
  }
  if (phase == 0 && now - phaseStart >= greenPattern[currentLane-1]) {
    setTL(currentLane, YELLOW);
    phase = 1;
    phaseStart = now;
  } else if (phase == 1 && now - phaseStart >= YELLOW_TIME) {
    setTL(currentLane, RED);
    crossVehicle(currentLane==1?lane1:(currentLane==2?lane2:lane3), 2);
    lastLane = currentLane;
    currentLane = currentLane % 3 + 1;
    phase = 0;
    phaseStart = now;
    setTL(currentLane, GREEN);
  }

  // Refresh TFT
  if (now - lastDisplay >= DISPLAY_INTERVAL) {
    updateLCD();
    lastDisplay = now;
  }
}

// --- Generate or drop vehicle ------------------------------------------
void generateVehicle(Lane &ln, int num) {
  if (ln.count < MAX_QUEUE) {
    ln.q[ln.count++] = { vehicleCounter, millis() };
    Serial.printf("%lu: gen %lu lane%d q=%d\n", millis(), vehicleCounter, num, ln.count);
    vehicleCounter++;
  } else {
    ln.dropped++;
    Serial.printf("%lu: drop lane%d dropped=%d\n", millis(), num, ln.dropped);
  }
}

// --- Markov arrivals ----------------------------------------------------
void updateMarkov(Lane &ln, int num) {
  unsigned long m = millis();
  if (m >= ln.nextGenTime) {
    generateVehicle(ln, num);
    if (ln.markovState) {
      ln.nextGenTime = m + MARKOV_BASE_INTERVAL;
      ln.markovState = random(100) < MARKOV_P_GREEN;
    } else {
      ln.nextGenTime = m + random(500,5001);
      ln.markovState = random(100) >= MARKOV_P_OFF;
    }
  }
}

// --- Optimize green times ----------------------------------------------
void optimizePattern() {
  int tot = lane1.count + lane2.count + lane3.count;
  if (!tot) return;
  int qs[3] = {lane1.count,lane2.count,lane3.count};
  for (int i=0; i<3; i++) {
    long g = (long)qs[i]*MAX_GREEN_TIME/tot;
    greenPattern[i] = constrain(g, MIN_GREEN_TIME, MAX_GREEN_TIME);
  }
  Serial.printf("%lu: optimized [%d,%d,%d]\n", millis(), greenPattern[0], greenPattern[1], greenPattern[2]);
}

// --- Traffic-light output ----------------------------------------------
void setTL(int tl, LightColor color) {
  int idx = tl-1;
  digitalWrite(TL_PINS[idx][RED],LOW);
  digitalWrite(TL_PINS[idx][GREEN],LOW);
  digitalWrite(TL_PINS[idx][YELLOW],LOW);
  if (color==RED)    digitalWrite(TL_PINS[idx][RED],HIGH);
  else if(color==GREEN)  digitalWrite(TL_PINS[idx][GREEN],HIGH);
  else { digitalWrite(TL_PINS[idx][RED],HIGH); digitalWrite(TL_PINS[idx][GREEN],HIGH); }
  currentTL[idx]=color;
  Serial.printf("%lu: TL%d=%s\n", millis(), tl, colorToString(color));
}

// --- Cross up to maxPass cars ------------------------------------------
void crossVehicle(Lane &ln, int maxPass) {
  for (int i=0; i<maxPass && ln.count>0; i++) {
    unsigned long id=ln.q[0].id;
    for(int j=1;j<ln.count;j++) ln.q[j-1]=ln.q[j];
    ln.count--;
    Serial.printf("%lu: cross %lu q=%d\n", millis(), id, ln.count);
  }
}

// --- TFT display update -----------------------------------------------
void updateLCD() {
  static char lastBuf[256]="";
  char buf[256];
  snprintf(buf,sizeof(buf),
    "Mode:%s Last:%d\n"
    "TL1:%s TL2:%s TL3:%s\n"
    "Q:%d,%d,%d D:%d,%d,%d\n"
    "P:%d,%d,%d",
    digitalRead(MODE_SWITCH)?"Manual":"Auto",
    lastLane,
    colorToString(currentTL[0]),
    colorToString(currentTL[1]),
    colorToString(currentTL[2]),
    lane1.count,lane2.count,lane3.count,
    lane1.dropped,lane2.dropped,lane3.dropped,
    greenPattern[0]/1000,greenPattern[1]/1000,greenPattern[2]/1000
  );
  if(strcmp(buf,lastBuf)!=0){ strcpy(lastBuf,buf);
    tft.fillRect(0,0,tft.width(),96,ILI9341_BLACK);
    tft.setCursor(0,0); tft.print(buf);
  }
}

// --- Convert color to text --------------------------------------------
const char* colorToString(LightColor c) {
  return c==RED?"RED":c==GREEN?"GREEN":"YELLOW";
}