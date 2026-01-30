#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG_SERIAL 0

#define PTT_PIN     4    // GP4
#define DISABLE_PIN 1    // GP1

#define SK6805_PIN    0    // SK6805 data
#define SK6805_COUNT  7    // total LEDs in chain

// Logical LED indices
#define LED_STATUS    0    // Disable / enable status
#define LED_MUTE      1    // Mute indicator
#define LED_KEYCAP    2    // Keycap LED (mirrors mute for now)

Adafruit_NeoPixel strip(SK6805_COUNT, SK6805_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastStatusSent = 0;
const unsigned long statusInterval = 1000; // every 1s

bool device_enabled = true;
bool ptt_state = false;
bool pcmute_state = true;

bool is_disabled = false;
bool booting = true;
bool host_ready = false;

unsigned long disableButtonPressTime = 0;
const unsigned long holdDuration = 100; // hold duration

unsigned long lastSparkStart = 0;
bool sparkOn = false;

const unsigned long sparkInterval = 2000; // 2 second
const unsigned long sparkDuration = 50;  // 50 ms flash


unsigned long lastSerialMessageTime = 0;
const unsigned long serialTimeout = 5000; // 5 seconds
bool serial_active = false;

// Colors for status
uint32_t COLOR_MUTED;
uint32_t COLOR_MIC_ON;
uint32_t COLOR_DISABLED;
uint32_t COLOR_STANDBY;
uint32_t COLOR_ENABLED;
uint32_t COLOR_STATUS_ENABLED;
uint32_t COLOR_STATUS_DISABLED;

// Global brightness scaling (0–255)
#define BRIGHTNESS_GLOBAL 155   // Start high for SK6805-14 (only for tuning)

// Per-LED scaling factors (0.0 – 1.0)
#define BRIGHTNESS_STATUS  0.6f
#define BRIGHTNESS_MUTE    1.0f
#define BRIGHTNESS_KEYCAP  1.0f

// Breathing LED control
bool breathingActive = false;
unsigned long lastBreath = 0;
const unsigned long breathInterval = 30;  // ~33 FPS
					  
// Color scaling helper
uint32_t scaleColor(uint32_t color, float scale) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8)  & 0xFF;
  uint8_t b = color & 0xFF;

  r = (uint8_t)(r * scale * BRIGHTNESS_GLOBAL / 255.0f);
  g = (uint8_t)(g * scale * BRIGHTNESS_GLOBAL / 255.0f);
  b = (uint8_t)(b * scale * BRIGHTNESS_GLOBAL / 255.0f);

  return strip.Color(r, g, b);
}


void updateDisabledSparkLED() {
  unsigned long now = millis();

  // Start a new spark
  if (!sparkOn && (now - lastSparkStart >= sparkInterval)) {
    sparkOn = true;
    lastSparkStart = now;
  }

  // End the spark
  if (sparkOn && (now - lastSparkStart >= sparkDuration)) {
    sparkOn = false;
  }

  uint32_t color;

  if (sparkOn) {
    // Bright green spark
    color = scaleColor(strip.Color(0, 80, 0), BRIGHTNESS_MUTE);
  } else {
    // Dim steady green
    color = scaleColor(strip.Color(0, 20, 0), BRIGHTNESS_MUTE);
  }

  strip.setPixelColor(LED_MUTE, color);
  strip.show();
}


void updateStatusLED() {

  // 1. LED_STATUS — device state
  if (device_enabled) {
    strip.setPixelColor(
      LED_STATUS,
      scaleColor(COLOR_STATUS_ENABLED, BRIGHTNESS_STATUS)
    );
  } else {
    strip.setPixelColor(
      LED_STATUS,
      scaleColor(COLOR_STATUS_DISABLED, BRIGHTNESS_STATUS)
    );
  }

  // 2. Device disabled: no local control
  if (!device_enabled) {
    breathingActive = false;
    return;
  }

  // 3. Standby / host-unknown state
  if (!serial_active || !host_ready) {
    breathingActive = true;
    return;
  }

  // 4. Host-connected, known mute state
  breathingActive = false;

  uint32_t baseMuteColor =
    pcmute_state ? COLOR_MUTED : COLOR_MIC_ON;

  strip.setPixelColor(
    LED_MUTE,
    scaleColor(baseMuteColor, BRIGHTNESS_MUTE)
  );
}

void updateKeycapLED() {
  if (!device_enabled) {
    strip.setPixelColor(LED_KEYCAP, 0);  // off
    return;
  }

  if (ptt_state) {
    strip.setPixelColor(
      LED_KEYCAP,
      scaleColor(COLOR_MIC_ON, BRIGHTNESS_KEYCAP)
    );
  } else {
    strip.setPixelColor(LED_KEYCAP, 0);  // off
  }
}

void updateBreathingLED() {
  if (!breathingActive) return;

  unsigned long now = millis();

  float phase = now / 1000.0f * PI;
  float breath = (sin(phase) + 1.0f) * 0.5f;  // 0..1

  uint8_t b = (uint8_t)(255 * breath);

  uint32_t c = scaleColor(strip.Color(0, 0, b), BRIGHTNESS_MUTE);

  strip.setPixelColor(LED_MUTE, c);
  strip.setPixelColor(LED_KEYCAP, c);
}

void sendStatusMessage(const char* msg) {
  if (!serial_active) return;  // only speak when spoken to

  if (Serial && Serial.dtr() && Serial.availableForWrite() > 0) {
    Serial.println(msg);
  }
}

void ledSelfTest() {
  strip.clear();
  strip.setPixelColor(LED_STATUS, strip.Color(255, 0, 0));
  strip.setPixelColor(LED_MUTE, strip.Color(0, 255, 0));
  strip.setPixelColor(LED_KEYCAP, strip.Color(0, 0, 255));
  strip.show();
  delay(1500);
  strip.clear();
  strip.show();
}

void setup() {
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(DISABLE_PIN, INPUT_PULLUP);

  strip.begin();

  ledSelfTest();

  strip.clear();

  // Initialize colors
  COLOR_MUTED    = strip.Color(28, 0, 0);
  COLOR_MIC_ON   = strip.Color(0, 28, 0);
  COLOR_DISABLED = strip.Color(0, 0, 0);
  COLOR_STANDBY  = strip.Color(0, 0, 25);
  COLOR_ENABLED  = strip.Color(0, 25, 0);
  COLOR_STATUS_ENABLED  = strip.Color(25, 25, 25);  // white
  COLOR_STATUS_DISABLED = strip.Color(25, 15, 0);   // amber


  // Explicit boot state
  strip.setPixelColor(LED_STATUS, COLOR_ENABLED);
  strip.setPixelColor(LED_MUTE, COLOR_STANDBY);
  strip.setPixelColor(LED_KEYCAP, COLOR_STANDBY);

  strip.show(); // Show blue on enabled immediately

  Serial.begin(115200);

  if (Serial && Serial.dtr()) {
    sendStatusMessage("Device booted");
    sendStatusMessage("VERSION: " FIRMWARE_VERSION);
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

  if (ptt_pressed != ptt_state) {
    ptt_state = ptt_pressed;

    updateKeycapLED();   // always local, always immediate

    if (device_enabled && serial_active) {
      if (ptt_state) {
        sendStatusMessage("PTT pressed");
      } else {
        sendStatusMessage("PTT released");
      }
    }

    updateStatusLED();   // mute LED may still depend on host state
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
      updateKeycapLED();
    } else if (cmd.equalsIgnoreCase("UNMUTE")) {
      //sendStatusMessage("Received UNMUTE from host");
      // set WS2812 to green
      host_ready = true;
      pcmute_state = false;
      updateStatusLED();
      updateKeycapLED();
    } else if (cmd.equalsIgnoreCase("ACK")) {
      #if DEBUG_SERIAL
        sendStatusMessage("Received ACK from host");
      #endif
      lastSerialMessageTime = millis();
      host_ready = true;
      Serial.println("ACK");
    } else if (cmd.equalsIgnoreCase("VERSION")) {
      sendStatusMessage("VERSION: " FIRMWARE_VERSION);
    }
  }

  
  // Handle disable button press and hold toggle
  static bool disableToggleLatched = false;

  if (disable_pressed && serial_active) {  // Only allow disable when host is connected
    if (disableButtonPressTime == 0) {
      disableButtonPressTime = millis();
      disableToggleLatched = false;
    } else if (!disableToggleLatched &&
             millis() - disableButtonPressTime >= holdDuration) {

      device_enabled = !device_enabled;

      if (device_enabled) {
        sendStatusMessage("Device enabled");
      } else {
        sendStatusMessage("Device disabled");
      }

      updateStatusLED();
      updateKeycapLED();

      disableToggleLatched = true;  // prevent retrigger
    }
  } else {
    disableButtonPressTime = 0;
    disableToggleLatched = false;
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
      // Auto-enable when host disconnects so device goes to standby mode
      if (!device_enabled) {
        device_enabled = true;
      }
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
    updateKeycapLED();   // clear frozen state
  }

  // Run breathing animation if enabled
  updateBreathingLED();

  // At the end of loop()
  if (!device_enabled && serial_active && host_ready) {
    updateDisabledSparkLED();
  }

  // Single strip.show() to prevent flickering
  strip.show();
}
