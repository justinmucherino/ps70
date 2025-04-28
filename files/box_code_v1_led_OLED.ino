/*  Punishment box v3  –  buzzer, LED (GPIO 19), OLED status (I²C 0x3C)  */
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

/* -------- Pins & timing constants -------- */
constexpr uint8_t PIEZO_PIN   = 4;      // passive PT-2726PQ
constexpr uint8_t REED_PIN    = 5;      // reed switch (LOW = lid shut)
constexpr uint8_t LED_PIN     = 19;     // slow/fast blink
constexpr uint16_t BUZZ_HZ    = 2500;   // alarm frequency
constexpr uint32_t ALARM_MS   = 60'000; // 60-second punishment
constexpr uint32_t DEBOUNCE_MS = 10;    // reed debounce
constexpr uint32_t BLINK_SLOW = 1'000;  // ms per toggle
constexpr uint32_t BLINK_FAST =   100;  // ms per toggle

/* -------- 128×64 I²C OLED on 0x3C -------- */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0,            // rotation
    /* reset = */ U8X8_PIN_NONE   // SSD1306 reset pin tied high on most modules
);

/* -------- State variables -------- */
bool      alarmActive  = false;
uint32_t  alarmStartMS = 0;
uint32_t  lastOpenMS   = 0;

bool      ledState     = false;
uint32_t  lastBlinkMS  = 0;
uint32_t  lastScreenMS = 0;

void setup() {
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(LED_PIN,  OUTPUT);

  /* Pre-allocate LEDC channel so first real tone() has zero lag */
  tone(PIEZO_PIN, BUZZ_HZ, 5);
  noTone(PIEZO_PIN);

  /* ---- OLED ---- */
  Wire.begin();                            // SDA 21, SCL 22
  u8g2.begin();
  u8g2.setI2CAddress(0x3C << 1);           // << 1 → 8-bit form for U8g2
  u8g2.setFont(u8g2_font_ncenB08_tr);
}

void loop() {
  uint32_t now       = millis();
  bool lidOpenRaw    = digitalRead(REED_PIN);          // HIGH = lid open
  static bool lidOpen = false;

  /* ---- debounce ---- */
  if (lidOpenRaw && !lidOpen) {
      if (now - lastOpenMS >= DEBOUNCE_MS) lidOpen = true;
  } else if (!lidOpenRaw) {
      lidOpen = false;
      lastOpenMS = now;
  }

  /* ---- alarm control ---- */
  if (!alarmActive && lidOpen) {
      alarmActive  = true;
      alarmStartMS = now;
      tone(PIEZO_PIN, BUZZ_HZ);
  }
  if (alarmActive) {
      bool timeout = (now - alarmStartMS >= ALARM_MS);
      bool lidShut = !lidOpen;
      if (timeout || lidShut) {
          noTone(PIEZO_PIN);
          alarmActive = false;
      }
  }

  /* ---- LED blink ---- */
  uint32_t period = alarmActive ? BLINK_FAST : BLINK_SLOW;
  if (now - lastBlinkMS >= period) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastBlinkMS = now;
  }

  /* ---- OLED update (every 250 ms) ---- */
  if (now - lastScreenMS >= 250) {
      lastScreenMS = now;
      u8g2.clearBuffer();
      if (alarmActive) {
          u8g2.setFont(u8g2_font_ncenB10_tr);
          u8g2.drawStr(10, 32, "BOX IS OPEN!");
      } else {
          u8g2.setFont(u8g2_font_ncenB08_tr);
          u8g2.drawStr(25, 30, "Box is shut");
          u8g2.drawStr(12, 50, "Good night :)");
      }
      u8g2.sendBuffer();
  }
}
