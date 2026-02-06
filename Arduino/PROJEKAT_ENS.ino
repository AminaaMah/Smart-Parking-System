#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>

#define I2C_ADDR 0x27
LiquidCrystal_I2C lcd(I2C_ADDR, 16, 2);
Servo rampServo;

// ----- RFID RC522 -----
#define SS_PIN   53
#define RST_PIN  49
MFRC522 rfid(SS_PIN, RST_PIN);

// ----- WHITELIST KARTICA -----
// Test UID-
const byte WHITELIST[][4] = {
  {0xFD, 0x1E, 0x37, 0x06},  // Kartica 1
  {0x46, 0x53, 0x19, 0x06}   // Kartica 2
};
const int WHITELIST_COUNT = 2;

// ----- PINOVI -----
const int redPins[3]   = {13, 10, 7};
const int greenPins[3] = {12, 9, 6};
const int btnPins[3]   = {11, 8, 5};

const int entryPin = 22;
const int exitPin  = 23;

// ===== PAYMENT + BUZZER + PAID LED =====
const int payPin     = 24;  // PAY button
const int buzzerPin  = 27;  // active buzzer 
const int paidLedPin = 28;  // optional PAID LED 

const int servoPin = 46;

bool busy[3] = {false, false, false};
bool lastBtn[3] = {HIGH, HIGH, HIGH};

bool lastEntryState = HIGH;
bool lastExitState  = HIGH;
bool lastPayState   = HIGH;

unsigned long rampLowerTime = 0;
bool rampIsLowered = true;
bool rampActive = false;

// payment state
bool needPay = false;
bool paid = false;
unsigned long entryStartMs = 0;

// RFID variables
unsigned long lastRfidRead = 0;
const unsigned long RFID_COOLDOWN = 1000; // 1 second cooldown

// ----- RFID FUNCTIONS -----
bool checkRFID() {
  // Check if new card is present
  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }
  
  // Verify if the card can be read
  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }
  
  // Display UID on Serial Monitor for debugging
  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
    content.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }
  Serial.println();
  
  // Check against whitelist
  bool authorized = false;
  for (int i = 0; i < WHITELIST_COUNT; i++) {
    bool match = true;
    for (byte j = 0; j < 4; j++) {
      if (rfid.uid.uidByte[j] != WHITELIST[i][j]) {
        match = false;
        break;
      }
    }
    if (match) {
      authorized = true;
      break;
    }
  }
  
  // Halt PICC
  rfid.PICC_HaltA();
  
  return authorized;
}

void beepShort() {
  digitalWrite(buzzerPin, HIGH);
  delay(120);
  digitalWrite(buzzerPin, LOW);
}

void beepLong() {
  digitalWrite(buzzerPin, HIGH);
  delay(400);
  digitalWrite(buzzerPin, LOW);
}

void beep(int duration, int times = 1) {
  for (int i = 0; i < times; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(duration);
    digitalWrite(buzzerPin, LOW);
    if (i < times - 1) delay(100);
  }
}

void updateLeds() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(redPins[i],   busy[i] ? HIGH : LOW);
    digitalWrite(greenPins[i], busy[i] ? LOW  : HIGH);
  }
}

void updateLcd() {
  lcd.clear();

  int freeCnt = 0;
  for (int i = 0; i < 3; i++) if (!busy[i]) freeCnt++;

  lcd.setCursor(0, 0);
  lcd.print("Free:");
  lcd.print(freeCnt);
  lcd.print("/3");

  lcd.setCursor(11, 0);
  lcd.print(rampIsLowered ? "[CLSD]" : "[OPEN]");

  if (freeCnt == 0) {
    lcd.setCursor(13, 0);
    lcd.print("FULL");
  }

  lcd.setCursor(0, 1);
  lcd.print("P1:");
  lcd.print(busy[0] ? "B " : "F ");

  lcd.setCursor(5, 1);
  lcd.print("P2:");
  lcd.print(busy[1] ? "B " : "F ");

  lcd.setCursor(10, 1);
  lcd.print("P3:");
  lcd.print(busy[2] ? "B" : "F");

  // PAID indikator (kratko)
  if (paid) {
    lcd.setCursor(15, 1);
    lcd.print("$");
  }
}

void showTempMsg(const char* l1, const char* l2, int ms) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(l1);
  lcd.setCursor(0,1); lcd.print(l2);
  delay(ms);
  updateLcd();
}

void showTempMsg(String l1, String l2, int ms) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(l1);
  lcd.setCursor(0,1); lcd.print(l2);
  delay(ms);
  updateLcd();
}

void openRamp() {
  if (rampIsLowered) {
    rampServo.write(90);
    rampIsLowered = false;
    rampActive = true;
    rampLowerTime = millis() + 5000;
    updateLcd();
  }
}

void closeRamp() {
  if (!rampIsLowered) {
    rampServo.write(0);
    rampIsLowered = true;
    rampActive = false;
    updateLcd();
  }
}

bool hasFreeSpot() {
  for (int i = 0; i < 3; i++) if (!busy[i]) return true;
  return false;
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  Serial.println("Parking System with RFID Starting...");
  
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Check RFID reader version
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("MFRC522 Version: 0x");
  Serial.print(version, HEX);
  
  if (version == 0x92) {
    Serial.println(" (v2.0)");
  } else if (version == 0x91 || version == 0x90) {
    Serial.println(" (v1.0)");
  } else {
    Serial.println(" (Unknown - check wiring!)");
  }
  
  rampServo.attach(servoPin);

  pinMode(buzzerPin, OUTPUT);
  pinMode(paidLedPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(paidLedPin, LOW);

  for (int i = 0; i < 3; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);
    pinMode(btnPins[i], INPUT_PULLUP);
  }

  pinMode(entryPin, INPUT_PULLUP);
  pinMode(exitPin, INPUT_PULLUP);
  pinMode(payPin, INPUT_PULLUP);

  closeRamp();

  updateLeds();
  updateLcd();
  
  // Show welcome message
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("RFID Parking");
  lcd.setCursor(0,1); lcd.print("System Ready");
  beep(100, 2);
  delay(1000);
  updateLcd();
  
  Serial.println("System ready. Scan RFID card to enter.");
}

void loop() {
  bool changed = false;
  unsigned long currentTime = millis();

  // --- 1) RFID ENTRY ---
  if (currentTime - lastRfidRead >= RFID_COOLDOWN) {
    if (checkRFID()) {
      lastRfidRead = currentTime;
      
      if (hasFreeSpot()) {
        // RFID authorized - create ticket
        needPay = true;
        paid = false;
        digitalWrite(paidLedPin, LOW);
        entryStartMs = currentTime;

        beep(200, 1);
        showTempMsg("RFID OK", "Welcome!", 800);
        openRamp();
        
        Serial.println("RFID authorized - entry granted");
      } else {
        beep(80, 3);
        showTempMsg("PARKING FULL", "Try later", 1200);
        Serial.println("RFID authorized but parking full");
      }
    }
  }

  // --- 2) ENTRY SENSOR (backup) ---
  bool currentEntryState = digitalRead(entryPin);
  if (lastEntryState == HIGH && currentEntryState == LOW) {
    delay(50);

    if (hasFreeSpot()) {
      // create "ticket" => must pay before exit
      needPay = true;
      paid = false;
      digitalWrite(paidLedPin, LOW);
      entryStartMs = currentTime;

      beepShort();
      openRamp();
      showTempMsg("ENTRY OK", "Ticket created", 800);
    } else {
      beepLong();
      showTempMsg("PARKING FULL", "Entry blocked", 900);
    }
  }
  lastEntryState = currentEntryState;

  // --- 3) PAY BUTTON ---
  bool currentPayState = digitalRead(payPin);
  if (lastPayState == HIGH && currentPayState == LOW) {
    delay(50);

    if (needPay && !paid) {
      unsigned long seconds = (currentTime - entryStartMs) / 1000;
      unsigned long priceKM = (seconds + 9) / 10; // 1KM per 10s 

      paid = true;
      needPay = false;
      digitalWrite(paidLedPin, HIGH);

      beepShort();
      char l2[17];
      snprintf(l2, sizeof(l2), "%lus  %luKM", seconds, priceKM);
      showTempMsg("PAYMENT OK", l2, 1400);
      
      Serial.print("Payment processed: ");
      Serial.print(seconds);
      Serial.print("s, ");
      Serial.print(priceKM);
      Serial.println("KM");
    } else {
      beepLong();
      showTempMsg("NO TICKET", "Press ENTRY", 1100);
    }
  }
  lastPayState = currentPayState;

  // --- 4) EXIT SENSOR (ONLY IF PAID) ---
  bool currentExitState = digitalRead(exitPin);
  if (lastExitState == HIGH && currentExitState == LOW) {
    delay(50);

    if (paid) {
      beepShort();
      openRamp();
      showTempMsg("EXIT OK", "Goodbye!", 800);

      // reset payment
      paid = false;
      needPay = false;
      digitalWrite(paidLedPin, LOW);
      Serial.println("Exit granted - payment cleared");
    } else {
      beepLong();
      showTempMsg("PAY FIRST", "Exit blocked", 900);
    }
  }
  lastExitState = currentExitState;

  // --- 5) AUTO CLOSE RAMP AFTER 5s ---
  if (!rampIsLowered && rampActive && currentTime >= rampLowerTime) {
    closeRamp();
  }

  // --- 6) PARKING SPOTS VIA BUTTONS ---
  for (int i = 0; i < 3; i++) {
    bool now = digitalRead(btnPins[i]);
    if (lastBtn[i] == HIGH && now == LOW) {
      delay(30);
      busy[i] = !busy[i];
      changed = true;
      
      Serial.print("Parking spot ");
      Serial.print(i+1);
      Serial.print(": ");
      Serial.println(busy[i] ? "BUSY" : "FREE");
    }
    lastBtn[i] = now;
  }

  if (changed) {
    updateLeds();
    updateLcd();
  }

  delay(50);
}

// ----- FUNKCIJA ZA DODAVANJE NOVIH KARTICA -----
void addNewCardToWhitelist() {
  
}