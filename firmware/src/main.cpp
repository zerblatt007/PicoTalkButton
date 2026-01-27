#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// FIRMWARE_VERSION is automatically injected by get_version.py at compile time
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

#define PTT_PIN     4    // GP4
#define DISABLE_PIN 1    // GP1
#define LED_PIN    13    // Built-in LED
#define LED_PIN_K   3    // Keycap LED
#define WS2812_PIN  0    // WS2812 data

#define WS2812_COUNT 1

Adafruit_NeoPixel strip(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastStatusSent = 0;
const unsigned long statusInterval = 1000; // every 1s

bool device_enabled = true;
bool ptt_state = false;
bool pcmute_state = true;

bool is_disabled = false;
bool booting = true;
bool host_ready = false;

unsigned long disableButtonPressTime = 0;
const unsigned long holdDuration = 200; // 2 seconds hold

unsigned long lastSerialMessageTime = 0;
const unsigned long serialTimeout = 5000; // 5 seconds
bool serial_active = false;

// Colors for WS2812 status
uint32_t COLOR_MUTED;
uint32_t COLOR_MIC_ON;
uint32_t COLOR_DISABLED;
uint32_t COLOR_STANDBY;

// Breathing LED control
bool breathingActive = false;
unsigned long lastBreath = 0;
const unsigned long breathInterval = 30;  // ~33 FPS

void updateStatusLED() {
  if (!serial_active && device_enabled) {
    // STANDBY mode (host not connected but device is enabled) -> breathe blue
    if (!breathingActive) {
      breathingActive = true;
      // Don't reset lastBreath - let animation continue smoothly
    }
    return;
  }

  // Disable breathing in all other cases
  if (breathingActive) {
    breathingActive = false;
  }

  // Determine color based on mute state
  uint32_t color = pcmute_state ? COLOR_MUTED : COLOR_MIC_ON;

  // Always show mute state color, even if device is disabled
  if (strip.getPixelColor(0) != color) {
    strip.setPixelColor(0, color);
    strip.show();
  }
}


void updateBreathingLED() {
  if (!breathingActive) return;

  unsigned long now = millis();
  if (now - lastBreath >= breathInterval) {
    lastBreath = now;

    float phase = now / 1000.0 * PI;  // ~1 cycle/sec
    float breath = (sin(phase) + 1.0) * 0.5;

    uint8_t b = static_cast<uint8_t>(8 * breath);  // blue intensity (0–8)
    strip.setPixelColor(0, strip.Color(0, 0, b));
    strip.show();
  }
}


void sendStatusMessage(const char* msg) {
  if (!serial_active || !host_ready) return;  // only speak when spoken to

  if (Serial && Serial.dtr() && Serial.availableForWrite() > 0) {
    Serial.println(msg);
  }
}

void setup() {
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(DISABLE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_K, OUTPUT);

  digitalWrite(LED_PIN, HIGH);  // Turn on built-in LED initially (powered)
  digitalWrite(LED_PIN_K, LOW);  // Turn on Keycap LED initially (powered)

  strip.begin();
  strip.setPixelColor(0, COLOR_STANDBY); 
  strip.show(); // Show blue immediately

  // Initialize colors
  COLOR_MUTED = strip.Color(8, 0, 0);
  COLOR_MIC_ON = strip.Color(0, 8, 0);
  COLOR_DISABLED = strip.Color(0, 0, 0);
  COLOR_STANDBY = strip.Color(0, 0, 5);

  Serial.begin(115200);

  if (Serial && Serial.dtr()) {
    sendStatusMessage("Device booted");
    sendStatusMessage("Device enabled");
  }

  serial_active = false;                 // assume inactive at boot
}

void loop() {
  // Read buttons (active low)
  bool ptt_pressed = digitalRead(PTT_PIN) == LOW;
  bool disable_pressed = digitalRead(DISABLE_PIN) == LOW;

  // Repeatedly send 'Device enabled'
  if (device_enabled && serial_active && millis() - lastStatusSent >= statusInterval) {
    sendStatusMessage("Device enabled");
    lastStatusSent = millis();
  }

  if (device_enabled && serial_active)  {
    // Handle PTT button state change
    if (ptt_pressed != ptt_state) {
      ptt_state = ptt_pressed;
      if (ptt_state) {
        sendStatusMessage("PTT pressed");
      } else {
      sendStatusMessage("PTT released");
      }
      updateStatusLED();
    }
  }

  // Read serial commands from host
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');  // read until newline
    cmd.trim();  // remove whitespace/newlines

    lastSerialMessageTime = millis();
    serial_active = Serial.dtr();

    if (cmd.equalsIgnoreCase("MUTE")) {
      //sendStatusMessage("Received MUTE from host");
      // set WS2812 to red
      host_ready = true;
      pcmute_state = true;
      updateStatusLED();
    } else if (cmd.equalsIgnoreCase("UNMUTE")) {
      //sendStatusMessage("Received UNMUTE from host");
      // set WS2812 to green
      host_ready = true;
      pcmute_state = false;
      updateStatusLED();
    } else if (cmd.equalsIgnoreCase("VERSION")) {
      Serial.print("Firmware version: ");
      Serial.println(FIRMWARE_VERSION);
      host_ready = true;
    } else {
//      sendStatusMessage("Unknown command: " + cmd);
//      sendStatusMessage((String("Unknown command: ") + cmd).c_str());
    }
  }

  
  // Handle disable button press and hold toggle
  if (disable_pressed) {
    if (disableButtonPressTime == 0) {
      disableButtonPressTime = millis();
    } else if (millis() - disableButtonPressTime >= holdDuration) {
      device_enabled = !device_enabled;
      if (device_enabled) {
        sendStatusMessage("Device enabled");
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(LED_PIN_K, LOW);
      } else {
        sendStatusMessage("Device disabled");
        digitalWrite(LED_PIN, LOW);
        digitalWrite(LED_PIN_K, HIGH);
      }
      updateStatusLED();
      // Debounce the toggle so it only triggers once per press
      while (digitalRead(DISABLE_PIN) == LOW) {
        delay(10);
      }
      disableButtonPressTime = 0;
    }
  } else {
    disableButtonPressTime = 0;
  }

  // Detect serial inactivity
  bool dtr = Serial && Serial.dtr();
  if (dtr != serial_active) {
    serial_active = dtr;

    if (serial_active) {
      // Only now do we start the handshake
      host_ready = false;  // require MUTE/UNMUTE from host
      // No message yet — just waiting for the host to send first
    } else {
      host_ready = false;
      updateStatusLED();  // Blue LED
    }
  }


  delay(10);

  if (booting && millis() > 1000) {
    booting = false;
    updateStatusLED(); // do the first real update now
  }

  if (serial_active && millis() - lastSerialMessageTime > serialTimeout) {
    serial_active = false;
    host_ready = false;
    sendStatusMessage("Host inactive -> standby mode");
    updateStatusLED();  // show blue
  }

  // Run breathing animation if enabled
  updateBreathingLED();


}

