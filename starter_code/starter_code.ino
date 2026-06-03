/*
 * IBET Security Pouch — Arduino R4 Prototype
 *
 * WIRING:
 *   MFRC522 RFID : SS=10, RST=5, MOSI=11, MISO=12, SCK=13
 *   Servo        : pin 9
 *   RGB LED      : R=6, G=7, B=8  (common-cathode; add 220Ω resistors)
 *   Buzzer       : pin 3
 *   Button       : pin 2 → GND  (uses internal pull-up)
 *
 * DEMO FLOW:
 *   1. Power on → GREEN (unlocked / unarmed)
 *   2. Scan tag  → enrolls tag, locks bag → RED (locked / armed)
 *   3. Press btn → ALARM (siren + flashing red/blue)
 *   4. Scan tag  → disarms + unlocks → GREEN
 *   5. Scan tag  → locks again → RED
 *   6. Wrong tag → brief BLUE flash, stays RED
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// ── Pin assignments ──────────────────────────────────────────────────────────
#define SS_PIN    10
#define RST_PIN    5
#define SERVO_PIN  9
#define LED_R      6
#define LED_G      7
#define LED_B      8
#define BUZZER     3
#define BUTTON     2

// ── Servo lock positions (degrees) ──────────────────────────────────────────
const int LOCKED_POS   = 90;
const int UNLOCKED_POS = 0;

// ── Alarm auto-shutoff ───────────────────────────────────────────────────────
const unsigned long ALARM_MAX_MS = 30000UL;  // 30 s

// ── Hardware objects ─────────────────────────────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);
Servo   lockServo;

// ── Enrolled RFID tag ────────────────────────────────────────────────────────
byte authUID[10];
byte authUIDLen = 0;
bool enrolled   = false;

// ── System state ─────────────────────────────────────────────────────────────
bool locked      = false;
bool alarmActive = false;

// ── Timing ───────────────────────────────────────────────────────────────────
unsigned long alarmStart = 0;
unsigned long lastBlink  = 0;
unsigned long lastBtnMs  = 0;
bool blinkOn = false;
bool lastBtn = HIGH;

// ── LED helpers ──────────────────────────────────────────────────────────────
void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}

// ── Lock/unlock ──────────────────────────────────────────────────────────────
void applyLocked() {
  locked = true;
  lockServo.write(LOCKED_POS);
  setLED(true, false, false);  // red
  Serial.println(">> LOCKED / ARMED");
}

void applyUnlocked() {
  locked      = false;
  alarmActive = false;
  noTone(BUZZER);
  lockServo.write(UNLOCKED_POS);
  setLED(false, true, false);  // green
  Serial.println(">> UNLOCKED / UNARMED");
}

// ── Alarm ────────────────────────────────────────────────────────────────────
void triggerAlarm() {
  if (!alarmActive) {
    alarmActive = true;
    alarmStart  = millis();
    Serial.println("!!! ALARM TRIGGERED — scan RFID to disarm !!!");
  }
}

void updateAlarm() {
  if (!alarmActive) return;

  // Auto-shutoff after ALARM_MAX_MS
  if (millis() - alarmStart > ALARM_MAX_MS) {
    alarmActive = false;
    noTone(BUZZER);
    setLED(true, false, false);  // back to red (still locked)
    Serial.println("Alarm auto-shutoff. Still locked — scan RFID to unlock.");
    return;
  }

  // Two-tone siren: alternates every 250 ms
  if ((millis() / 250) % 2 == 0) tone(BUZZER, 2000);
  else                            tone(BUZZER, 1000);

  // Flash red / blue every 150 ms
  if (millis() - lastBlink > 150) {
    lastBlink = millis();
    blinkOn   = !blinkOn;
    setLED(blinkOn, false, !blinkOn);
  }
}

// ── Button ───────────────────────────────────────────────────────────────────
void handleButton() {
  bool btn = digitalRead(BUTTON);
  // Falling edge with 200 ms debounce
  if (btn == LOW && lastBtn == HIGH && millis() - lastBtnMs > 200) {
    lastBtnMs = millis();
    if (locked) {
      triggerAlarm();
    } else {
      Serial.println("Bag is unarmed — lock it first to arm the alarm.");
    }
  }
  lastBtn = btn;
}

// ── RFID ─────────────────────────────────────────────────────────────────────
bool uidMatches() {
  if (rfid.uid.size != authUIDLen) return false;
  for (byte i = 0; i < authUIDLen; i++) {
    if (rfid.uid.uidByte[i] != authUID[i]) return false;
  }
  return true;
}

void printUID() {
  Serial.print("Scanned UID:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
}

void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  printUID();

  if (!enrolled) {
    // First scan: register this tag as the owner key
    authUIDLen = rfid.uid.size;
    for (byte i = 0; i < authUIDLen; i++) authUID[i] = rfid.uid.uidByte[i];
    enrolled = true;
    Serial.println("Tag enrolled! Locking bag...");
    applyLocked();

  } else if (uidMatches()) {
    if (locked) applyUnlocked();
    else        applyLocked();

  } else {
    Serial.println("Unknown tag — access denied.");
    // Brief blue flash then restore status color
    setLED(false, false, true);
    delay(300);
    if (locked) setLED(true, false, false);
    else        setLED(false, true, false);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ── Setup / Loop ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lockServo.attach(SERVO_PIN);

  pinMode(LED_R,  OUTPUT);
  pinMode(LED_G,  OUTPUT);
  pinMode(LED_B,  OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  applyUnlocked();
  Serial.println("=== IBET Security Pouch Prototype ===");
  Serial.println("Scan your RFID tag to enroll and lock the bag.");
}

void loop() {
  handleButton();
  handleRFID();
  updateAlarm();
}
