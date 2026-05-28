#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#include <TeensyDMX.h>


#ifdef ARDUINO_TEENSY36
#include <usb_midi.h>
extern usb_midi_class usbMIDI;
#endif

// Teensy USB MIDI is built-in; no extra include needed

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define OLED_ADDR     0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DMX config
#define DMX_CHANNELS  13  // 1-13 (CC7-CC19)
#define DMX_SERIAL    Serial1
qindesign::teensydmx::Sender dmx(DMX_SERIAL);

// MIDI config
#define MIDI_CC_START 7
#define MIDI_CC_END   19

uint8_t dmxValues[DMX_CHANNELS + 1] = {0}; // DMX channels are 1-based
static uint32_t ledOffTime = 0;

void showStatus(byte midiCh, byte cc, byte val, byte dmxCh, byte dmxVal); // Forward declaration

void setup() {
  // OLED
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("DMX MIDI Bridge");
  display.display();

  // DMX
  dmx.begin();
  for (int i = 1; i <= DMX_CHANNELS; i++) dmx.set(i, 0);

  // Teensy 3.6 onboard LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  while (usbMIDI.read()) {
    if (usbMIDI.getType() == 0xB0) { // 0xB0 = Control Change for channel 1, but getType() returns type only
      byte channel = usbMIDI.getChannel();
      byte control = usbMIDI.getData1();
      byte value = usbMIDI.getData2();
      if (control >= MIDI_CC_START && control <= MIDI_CC_END) {
        uint8_t dmxChannel = control - MIDI_CC_START + 1;
        dmxValues[dmxChannel] = map(value, 0, 127, 0, 255);
        dmx.set(dmxChannel, dmxValues[dmxChannel]);
        showStatus(channel, control, value, dmxChannel, dmxValues[dmxChannel]);
        digitalWrite(LED_BUILTIN, HIGH);
        ledOffTime = millis() + 50;
      }
    }
  }
  // Optionally, turn off LED if it was turned on
  if (ledOffTime && millis() > ledOffTime) {
    digitalWrite(LED_BUILTIN, LOW);
    ledOffTime = 0;
  }
}

// No OnControlChange needed; handled in loop()

void showStatus(byte midiCh, byte cc, byte val, byte dmxCh, byte dmxVal) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("MIDI Ch: "); display.println(midiCh);
  display.print("CC: "); display.print(cc);
  display.print(" -> DMX: "); display.println(dmxCh);
  display.print("Val: "); display.print(val);
  display.print(" -> "); display.println(dmxVal);
  display.display();
}
