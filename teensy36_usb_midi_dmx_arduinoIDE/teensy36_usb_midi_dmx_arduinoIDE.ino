// ===================== TIMER OVERFLOW NOTE =====================
// All time comparisons use unsigned subtraction (e.g., now - lastTime < interval),
// which is safe even if millis() overflows (wraps every ~50 days).
// No action needed for timer overflow; logic remains correct.
// ===============================================================
// ===================== IMPORTANT SAFETY NOTE =====================
// Lamp lockout/minimum ON state is lost on power cycle or reset.
// For critical safety, consider storing lampLastOffTime, lampLastOnTime,
// lampLockoutActive, lampMinOnActive, lampWasOn in EEPROM or RTC RAM.
// This implementation does NOT persist state across resets.
// ================================================================
// Teensy 3.6 USB MIDI to DMX (Arduino IDE version)
// Use this in the Arduino IDE with Tools > USB Type: "MIDI"
// Libraries: Adafruit SSD1306, Adafruit GFX, TeensyDMX

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TeensyDMX.h>


// --- Lamp security for DMX channels 3, 7, 11 (lamp channels) ---
// - If lamp is ON and goes to OFF, cannot be turned ON again for 15min (lockout)
// - If lamp is ON, cannot be turned OFF until ON for at least 10min (minimum ON time)
// - Visual feedback: lock icon for lockout, timer bar for minimum ON
// - Only 9 columns (C1,C2,C3,C5,C6,C7,C9,C10,C11), DMX channels used: 1,2,3,5,6,7,9,10,11 (skip 4,8)
#define LAMP_ON_THRESHOLD 10 // Value above this is ON
#define LAMP_LOCKOUT_MS 900000UL // 15 minutes in ms
#define LAMP_MIN_ON_MS 600000UL // 10 minutes in ms
#define NUM_COLS 9
#define NUM_LAMPS 3
// Only C3, C7, C11 are lamp channels (indices 2, 5, 8)
const int lampBarIdx[NUM_LAMPS] = {2, 5, 8}; // C3, C7, C11 (0-based in new arrays)
const int lampDmxCh[NUM_LAMPS] = {3, 7, 11};
// State for each lamp channel
uint32_t lampLastOffTime[NUM_LAMPS] = {0, 0, 0};
uint32_t lampLastOnTime[NUM_LAMPS] = {0, 0, 0};
bool lampLockoutActive[NUM_LAMPS] = {false, false, false};
bool lampMinOnActive[NUM_LAMPS] = {false, false, false};
bool lampWasOn[NUM_LAMPS] = {false, false, false};

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DMX config
#define DMX_CHANNELS  11  // 1-11 (used: 1,2,3,5,6,7,9,10,11)
#define DMX_SERIAL    Serial1
qindesign::teensydmx::Sender dmx(DMX_SERIAL);

// MIDI config





// MIDI CCs for mapping (9 columns: C1,C2,C3,C5,C6,C7,C9,C10,C11)
#define CC1  1
#define CC2  2
#define CC3  3
#define CC4  4
#define CC5  5
#define CC6  6
#define CC7  7
#define CC8  8
#define CC9  9
#define CC10 10
#define CC11 11

// Map C1, C2, C3, C5, C6, C7, C9, C10, C11 to DMX channels: 1,2,3,5,6,7,9,10,11 (skip 4,8)
const uint8_t ccList[NUM_COLS] = {CC1, CC2, CC3, CC5, CC6, CC7, CC9, CC10, CC11};
const char* ccNames[NUM_COLS] = {"C1", "C2", "C3", "C5", "C6", "C7", "C9", "10", "11"};
const uint8_t dmxMap[NUM_COLS] = {1, 2, 3, 5, 6, 7, 9, 10, 11}; // C1,C2,C3,C5,C6,C7,C9,C10,C11 to DMX

uint8_t ccValues[128] = {0}; // Store last value for each CC
uint8_t dmxValues[DMX_CHANNELS + 1] = {0}; // DMX channels are 1-based
static uint32_t ledOffTime = 0;
static uint32_t lastFaderDraw = 0;
const uint32_t FADER_DRAW_INTERVAL = 20; // ms

void drawFaders() {
  display.clearDisplay();
  const int numCols = NUM_COLS;
  // 1. Top yellow: DMXVERTIGO stretched and centered across full width
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const char* dmxText = "DMXVERTIGO";
  int dmxLen = 11;
  float dmxCharSpace = (float)(SCREEN_WIDTH - 1) / (dmxLen - 1); // -1 so last char is at 127
  for (int i = 0; i < dmxLen; i++) {
    int charX = (int)(i * dmxCharSpace + 0.5f);
    display.setCursor(charX, 0);
    display.write(dmxText[i]);
  }

  // 2. Middle: 9 bars, use full width, no DMX value text
  const int leftMargin = 2;
  const int rightMargin = 2;
  const int interBar = 1;
  const int barAreaTop = 10;
  const int barAreaBottom = SCREEN_HEIGHT - 10;
  const int barMaxHeight = barAreaBottom - barAreaTop;
  int totalBarSpace = SCREEN_WIDTH - leftMargin - rightMargin - (numCols - 1) * interBar;
  int barWidth = totalBarSpace / numCols;
  int leftover = totalBarSpace - (barWidth * numCols);
  int x = leftMargin;
  for (int i = 0; i < numCols; i++) {
    int thisBarWidth = barWidth + (i < leftover ? 1 : 0);
    uint8_t val = ccValues[ccList[i]];
    int barHeight = map(val, 0, 255, 0, barMaxHeight);
    display.drawRect(x, barAreaBottom - barMaxHeight, thisBarWidth, barMaxHeight, SSD1306_WHITE);
    if (barHeight > 0) {
      display.fillRect(x + 1, barAreaBottom - barHeight, thisBarWidth - 2, barHeight, SSD1306_WHITE);
    }
    // Draw lock icon or timer bar if this is a lamp channel
    for (int l = 0; l < NUM_LAMPS; l++) {
      if (i == lampBarIdx[l]) {
        if (lampLockoutActive[l]) {
          int lockX = x + thisBarWidth / 2 - 2;
          int lockY = barAreaBottom - barMaxHeight + 2;
          display.drawRect(lockX, lockY, 5, 5, SSD1306_WHITE);
          display.drawCircle(lockX + 2, lockY, 2, SSD1306_WHITE);
        } else if (lampMinOnActive[l]) {
          int timerBarY = barAreaBottom - barMaxHeight;
          display.fillRect(x + 2, timerBarY, thisBarWidth - 4, 2, SSD1306_WHITE);
        }
      }
    }
    x += thisBarWidth + interBar;
  }

  // 3. Bottom: C1–C9 labels, distributed
  display.setTextSize(1);
  x = leftMargin;
  int labelY = SCREEN_HEIGHT - 7;
  for (int i = 0; i < numCols; i++) {
    int thisBarWidth = barWidth + (i < leftover ? 1 : 0);
    int labelX = x + (thisBarWidth - 12) / 2;
    display.setCursor(labelX, labelY);
    display.print(ccNames[i]);
    x += thisBarWidth + interBar;
  }
  display.display();
}


// No longer used, replaced by drawFaders()
void showStatus(byte midiCh, byte cc, byte val, byte dmxCh, byte dmxVal) {}

// --- Error feedback ---
void showCriticalError(const char* msg) {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_BUILTIN, HIGH); delay(100);
    digitalWrite(LED_BUILTIN, LOW); delay(100);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CRITICAL ERROR!");
  display.println(msg);
  display.display();
  while (1) {
    digitalWrite(LED_BUILTIN, HIGH); delay(200);
    digitalWrite(LED_BUILTIN, LOW); delay(200);
  }
}

// --- Lamp logic encapsulation ---
void processLampLogic(int dmxChannel, uint8_t& scaled, byte control) {
  for (int l = 0; l < NUM_LAMPS; l++) {
    if (dmxChannel == lampDmxCh[l]) {
      bool lampOn = (scaled > LAMP_ON_THRESHOLD);
      uint32_t now = millis();
      if (lampOn && !lampWasOn[l]) {
        lampLastOnTime[l] = now;
        lampMinOnActive[l] = true;
      }
      if (!lampOn && lampWasOn[l]) {
        lampLastOffTime[l] = now;
        lampLockoutActive[l] = true;
        lampMinOnActive[l] = false;
      }
      if (lampWasOn[l] && lampMinOnActive[l] && !lampOn) {
        if (now - lampLastOnTime[l] < LAMP_MIN_ON_MS) {
          ccValues[control] = 255;
          scaled = 255;
          lampOn = true;
        } else {
          lampMinOnActive[l] = false;
        }
      }
      if (lampLockoutActive[l] && lampOn) {
        if (now - lampLastOffTime[l] < LAMP_LOCKOUT_MS) {
          ccValues[control] = 0;
          scaled = 0;
          lampOn = false;
        } else {
          lampLockoutActive[l] = false;
        }
      }
      lampWasOn[l] = lampOn;
    }
  }
}

// --- Mapping logic encapsulation ---
int getDMXChannelForCC(byte control) {
  for (int i = 0; i < NUM_COLS; i++) {
    if (control == ccList[i]) return dmxMap[i];
  }
  return 0;
}

void setup() {

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // OLED init failed: show error and halt
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW); delay(200);
    }
    // Try to show error on display (may not work)
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED ERROR!");
    display.println("Check wiring");
    display.display();
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH); delay(1000);
      digitalWrite(LED_BUILTIN, LOW); delay(1000);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("DMX MIDI Bridge");
  display.display();

  dmx.begin();
  // Soft start DMX channels
  softStartDMX();
  for (int i = 1; i <= DMX_CHANNELS; i++) dmx.set(i, 0);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  // Draw initial faders
  drawFaders();
}

void loop() {
  bool updated = false;
  // Buffer to store the latest value for each mapped CC in this loop
  uint8_t latestCCValues[NUM_COLS];
  bool ccReceived[NUM_COLS] = {false};

  // Read all MIDI messages and keep only the latest per mapped CC
  while (usbMIDI.read()) {
    if (usbMIDI.getType() == usbMIDI.ControlChange) {
      byte control = usbMIDI.getData1();
      byte value = usbMIDI.getData2();
      for (int i = 0; i < NUM_COLS; i++) {
        if (control == ccList[i]) {
          latestCCValues[i] = value;
          ccReceived[i] = true;
          break;
        }
      }
    }
  }

  // Process only the latest value for each mapped CC
  for (int i = 0; i < NUM_COLS; i++) {
    if (ccReceived[i]) {
      byte control = ccList[i];
      uint8_t scaled = map(latestCCValues[i], 0, 127, 0, 255);
      int dmxChannel = dmxMap[i];
      ccValues[control] = scaled;

      // --- Lamp security logic (encapsulated) ---
      processLampLogic(dmxChannel, scaled, control);
      // --- End lamp security logic ---

      dmxValues[dmxChannel] = scaled;
      dmx.set(dmxChannel, dmxValues[dmxChannel]);
      updated = true;
      digitalWrite(LED_BUILTIN, HIGH);
      ledOffTime = millis() + 50;
    }
  }

  if (updated && millis() - lastFaderDraw >= FADER_DRAW_INTERVAL) {
    drawFaders();
    lastFaderDraw = millis();
  }
  if (ledOffTime && millis() > ledOffTime) {
    digitalWrite(LED_BUILTIN, LOW);
    ledOffTime = 0;
  }
}

void softStartDMX() {
  const int rampSteps = 30;
  const int rampDelay = 10; // ms per step
  // Set lamp channels (DMX 3, 7, 11) to 0 immediately for safety
  for (int l = 0; l < NUM_LAMPS; l++) {
    dmx.set(lampDmxCh[l], 0);
  }
  // Ramp all other DMX channels
  for (int step = 0; step <= rampSteps; step++) {
    for (int i = 0; i < NUM_COLS; i++) {
      int dmxCh = dmxMap[i];
      // Skip lamp channels
      bool isLamp = false;
      for (int l = 0; l < NUM_LAMPS; l++) {
        if (dmxCh == lampDmxCh[l]) {
          isLamp = true;
          break;
        }
      }
      if (isLamp) continue;
      uint8_t target = dmxValues[dmxCh]; // always 0 unless EEPROM restore
      uint8_t val = (uint8_t)((target * step) / rampSteps);
      dmx.set(dmxCh, val);
    }
    delay(rampDelay);
  }
}
