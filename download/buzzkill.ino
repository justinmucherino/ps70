#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

/* ---------- pin map ---------- */
constexpr uint8_t SERVO_PIN = 18;   // signal (orange)
constexpr uint8_t PIEZO_PIN = 4;
constexpr uint8_t REED_PIN  = 5;
constexpr uint8_t LED_PIN   = 19;
constexpr uint8_t BTN_A     = 14;   // white
constexpr uint8_t BTN_B     = 27;   // red
constexpr uint8_t BTN_C     = 26;   // black

/* ---------- constants ---------- */
constexpr uint16_t BUZZ_HZ     = 2500;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t BLINK_SLOW  = 1000;
constexpr uint32_t BLINK_FAST  =  100;
constexpr uint8_t  LOCK_POS    = 90;    // absolute lock angle

/* ---------- objects ---------- */
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo latch;

/* ---------- state ---------- */
enum Mode { NORMAL, SET_H, SET_M, SET_S, COUNTDOWN, ALARM };
enum TimerType { NONE, ALRM, LOCK };
Mode      mode       = NORMAL;
TimerType timerType  = NONE;

uint8_t   setH = 0, setM = 0, setS = 0;
uint32_t  targetMs   = 0;

uint32_t  lastBlink  = 0;
uint32_t  lastScreen = 0;
bool      ledState   = false;
bool      blinkOn    = true;
uint32_t  lastBtnMs[3] = {0};

uint8_t   homeAngle  = 0;     // captured on every reboot

/* ---------- helpers ---------- */
bool btn(uint8_t pin, uint8_t idx) {
  if (digitalRead(pin) == LOW &&
      millis() - lastBtnMs[idx] > DEBOUNCE_MS) {
    lastBtnMs[idx] = millis();
    return true;
  }
  return false;
}

void showLCD() {
  lcd.clear();
  if (mode == NORMAL) {
    lcd.setCursor(0,0); lcd.print("Box is shut");
    lcd.setCursor(0,1); lcd.print("Good night :)");
    return;
  }
  if (mode == COUNTDOWN) {
    uint32_t left = (targetMs > millis()) ? targetMs - millis() : 0;
    uint8_t  h = left / 3600000UL;
    uint8_t  m = (left /  60000UL) % 60;
    uint8_t  s = (left /   1000UL) % 60;
    char buf[17]; sprintf(buf,"%02u:%02u:%02u",h,m,s);
    lcd.setCursor(0,0);
    lcd.print(timerType==ALRM ? "Time left" : "Unlocks in");
    lcd.setCursor(0,1); lcd.print(buf);
    return;
  }
  if (mode == ALARM) {
    lcd.setCursor(0,0); lcd.print("!! BOX IS OPEN");
    lcd.setCursor(0,1); lcd.print("Put phone back!");
    return;
  }
  /* SET screens */
  char buf[17]; sprintf(buf,"%02u:%02u:%02u",setH,setM,setS);
  lcd.setCursor(0,0);
  lcd.print(timerType==ALRM ? "Set Alarm:" : "Set Lock:");
  lcd.setCursor(0,1);
  for (uint8_t i=0;i<8;i++) {
    bool blinkField =
      (mode==SET_H && i<2) ||
      (mode==SET_M && i>=3 && i<5) ||
      (mode==SET_S && i>=6);
    lcd.print(blinkField && !blinkOn ? ' ' : buf[i]);
  }
}

/* ---------- setup ---------- */
void setup() {
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(LED_PIN,  OUTPUT);
  for (uint8_t p : {BTN_A, BTN_B, BTN_C}) pinMode(p, INPUT_PULLUP);

  tone(PIEZO_PIN, BUZZ_HZ, 5);   // reserve PWM
  noTone(PIEZO_PIN);

  Wire.begin(); lcd.init(); lcd.backlight();

  latch.attach(SERVO_PIN, 500, 2500);
  delay(200);                    // let servo settle
  homeAngle = latch.read();      // record initial position

  showLCD();
}

/* ---------- loop ---------- */
void loop() {
  uint32_t now = millis();

  /* LED + cursor blink */
  uint32_t blinkPeriod = (mode==ALARM) ? BLINK_FAST : BLINK_SLOW;
  if (now - lastBlink >= blinkPeriod) {
    lastBlink = now;
    ledState = !ledState; digitalWrite(LED_PIN, ledState);
    blinkOn  = !blinkOn;
    if (mode==SET_H||mode==SET_M||mode==SET_S) showLCD();
  }

  /* buttons */
  bool a = btn(BTN_A,0);
  bool b = btn(BTN_B,1);
  bool c = btn(BTN_C,2);

  /* enter set modes */
  if (mode == NORMAL) {
    if (digitalRead(BTN_A)==LOW && digitalRead(BTN_B)==LOW) {
      while(digitalRead(BTN_A)==LOW||digitalRead(BTN_B)==LOW); delay(50);
      mode = SET_H; timerType = ALRM; setH=setM=setS=0; showLCD(); return;
    }
    if (digitalRead(BTN_B)==LOW && digitalRead(BTN_C)==LOW) {
      while(digitalRead(BTN_B)==LOW||digitalRead(BTN_C)==LOW); delay(50);
      mode = SET_H; timerType = LOCK; setH=setM=setS=0; showLCD(); return;
    }
  }

  /* setting UI */
  if (mode==SET_H||mode==SET_M||mode==SET_S) {
    if (a) {
      if      (mode==SET_H) setH = (setH==10)?0:setH+1;
      else if (mode==SET_M) setM = (setM==59)?0:setM+1;
      else                  setS = (setS==59)?0:setS+1;
      showLCD();
    }
    if (b) {
      if      (mode==SET_H) mode = SET_M;
      else if (mode==SET_M) mode = SET_S;
      else {
        uint32_t span = (uint32_t)setH*3600000UL + setM*60000UL + setS*1000UL;
        if (span == 0) { mode = NORMAL; timerType = NONE; }
        else {
          targetMs = now + span;
          mode = COUNTDOWN;
          if (timerType == LOCK) latch.write(LOCK_POS); // lock to 90°
        }
        showLCD();
      }
    }
    if (c) { mode = NORMAL; timerType = NONE; showLCD(); }
    return;
  }

  /* countdown */
  if (mode == COUNTDOWN) {
    if (now > targetMs) {
      if (timerType == LOCK) latch.write(homeAngle);   // unlock
      mode = NORMAL; timerType = NONE; showLCD();
    }
    else if (timerType == ALRM && digitalRead(REED_PIN)==HIGH) {
      mode = ALARM; tone(PIEZO_PIN, BUZZ_HZ); showLCD();
    }
    if (now - lastScreen >= 250) { lastScreen = now; showLCD(); }
    return;
  }

  /* alarm */
  if (mode == ALARM && digitalRead(REED_PIN) == LOW) {
    noTone(PIEZO_PIN); mode = NORMAL; timerType = NONE; showLCD();
  }
}
