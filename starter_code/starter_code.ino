/*
 * IBET Security Pouch — Arduino R4 Prototype (RFID + Motion + Logging)
 *
 * FEATURES:
 *  - RFID lock/unlock system
 *  - Servo locking mechanism
 *  - RGB status LED
 *  - Button-triggered alarm
 *  - Buzzer siren
 *  - MPU6050 motion detection (GY-87)
 *  - LIVE accelerometer logging to Serial Monitor
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <math.h>

// ── RFID ─────────────────────────────────────────────
#define SS_PIN    10
#define RST_PIN    5

// ── Outputs ──────────────────────────────────────────
#define SERVO_PIN  9
#define LED_R      6
#define LED_G      7
#define LED_B      8
#define BUZZER     3
#define BUTTON     2

// ── Servo positions ──────────────────────────────────
const int LOCKED_POS   = 90;
const int UNLOCKED_POS = 0;

// ── Alarm settings ───────────────────────────────────
const unsigned long ALARM_MAX_MS = 30000UL;
const float MOTION_THRESHOLD = 0.35;

// ── Hardware objects ─────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);
Servo lockServo;
MPU6050 mpu(Wire);

// ── RFID storage ─────────────────────────────────────
byte authUID[10];
byte authUIDLen = 0;
bool enrolled = false;

// ── State ────────────────────────────────────────────
bool locked = false;
bool alarmActive = false;

// ── Timing ───────────────────────────────────────────
unsigned long alarmStart = 0;
unsigned long lastBlink = 0;
unsigned long lastBtnMs = 0;
unsigned long lastLogMs = 0;

bool blinkOn = false;
bool lastBtn = HIGH;

// ── Motion baseline ──────────────────────────────────
float baseAccel = 1.0;

// ── LED helper ───────────────────────────────────────
void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r);
  digitalWrite(LED_G, g);
  digitalWrite(LED_B, b);
}

// ── Lock / Unlock ────────────────────────────────────
void applyLocked() {
  locked = true;
  lockServo.write(LOCKED_POS);
  setLED(true, false, false);
  Serial.println(">> LOCKED / ARMED");
}

void applyUnlocked() {
  locked = false;
  alarmActive = false;
  noTone(BUZZER);
  lockServo.write(UNLOCKED_POS);
  setLED(false, true, false);
  Serial.println(">> UNLOCKED / UNARMED");
}

// ── Alarm ────────────────────────────────────────────
void triggerAlarm() {
  if (!alarmActive) {
    alarmActive = true;
    alarmStart = millis();
    Serial.println("!!! ALARM TRIGGERED !!!");
  }
}

void updateAlarm() {
  if (!alarmActive) return;

  if (millis() - alarmStart > ALARM_MAX_MS) {
    alarmActive = false;
    noTone(BUZZER);
    setLED(true, false, false);
    Serial.println("Alarm auto-shutoff (still locked)");
    return;
  }

  if ((millis() / 250) % 2 == 0) tone(BUZZER, 2000);
  else tone(BUZZER, 1000);

  if (millis() - lastBlink > 150) {
    lastBlink = millis();
    blinkOn = !blinkOn;
    setLED(blinkOn, false, !blinkOn);
  }
}

// ── Button ───────────────────────────────────────────
void handleButton() {
  bool btn = digitalRead(BUTTON);

  if (btn == LOW && lastBtn == HIGH && millis() - lastBtnMs > 200) {
    lastBtnMs = millis();

    if (locked) {
      triggerAlarm();
    } else {
      Serial.println("Lock bag first to arm alarm.");
    }
  }

  lastBtn = btn;
}

// ── RFID ─────────────────────────────────────────────
bool uidMatches() {
  if (rfid.uid.size != authUIDLen) return false;

  for (byte i = 0; i < authUIDLen; i++) {
    if (rfid.uid.uidByte[i] != authUID[i]) return false;
  }

  return true;
}

void printUID() {
  Serial.print("UID:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(" ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
}

void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  printUID();

  if (!enrolled) {
    authUIDLen = rfid.uid.size;
    for (byte i = 0; i < authUIDLen; i++)
      authUID[i] = rfid.uid.uidByte[i];

    enrolled = true;
    Serial.println("Tag enrolled");
    applyLocked();

  } else if (uidMatches()) {

    if (locked) applyUnlocked();
    else applyLocked();

  } else {
    Serial.println("Wrong tag");

    setLED(false, false, true);
    delay(300);

    if (locked) setLED(true, false, false);
    else setLED(false, true, false);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ── Motion detection ────────────────────────────────
void handleMotion() {
  if (!locked) return;

  mpu.update();

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();

  float totalAccel = sqrt(ax * ax + ay * ay + az * az);
  float delta = abs(totalAccel - baseAccel);

  // Trigger alarm if moved too much
  if (!alarmActive && delta > MOTION_THRESHOLD) {
    Serial.println("Motion detected!");
    triggerAlarm();
  }
}

// ── ACCELEROMETER LOGGING ───────────────────────────
void logAccelerometer() {
  if (millis() - lastLogMs < 200) return; // 5 Hz logging
  lastLogMs = millis();

  mpu.update();

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();

  float total = sqrt(ax * ax + ay * ay + az * az);

  Serial.print("ACC -> X:");
  Serial.print(ax, 3);
  Serial.print(" Y:");
  Serial.print(ay, 3);
  Serial.print(" Z:");
  Serial.print(az, 3);
  Serial.print(" | Total:");
  Serial.println(total, 3);
}

// ── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  SPI.begin();
  rfid.PCD_Init();

  Wire.begin();

  mpu.begin();
  mpu.calcOffsets(true, true);

  baseAccel = 1.0;

  lockServo.attach(SERVO_PIN);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  applyUnlocked();

  Serial.println("=== IBET Security Pouch Ready ===");
  Serial.println("Accelerometer logging enabled (5 Hz)");
}

// ── Loop ─────────────────────────────────────────────
void loop() {
  handleButton();
  handleRFID();
  updateAlarm();
  handleMotion();

  // LIVE sensor output
  logAccelerometer();
}