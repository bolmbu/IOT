#include <Arduino.h>
#include "AdafruitIO_WiFi.h"

// ===============================
// [1] Adafruit IO & WiFi Setting
// ===============================
#define IO_USERNAME  "kanka"

// ตอนอัป GitHub อย่าใส่ Key จริง
// ตอนรันจริงค่อยเปลี่ยนเป็น Adafruit IO Key ของตัวเอง
#define IO_KEY       "ADA_KEY"

#define WIFI_SSID    "Wokwi-GUEST"
#define WIFI_PASS    ""

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ===============================
// [2] Adafruit IO Feeds
// ===============================
AdafruitIO_Feed *levelFeed = io.feed("water-level");
AdafruitIO_Feed *oxygenFeed = io.feed("oxygen-level");

AdafruitIO_Feed *pumpRelayFeed = io.feed("pump-relay-status");
AdafruitIO_Feed *aeratorRelayFeed = io.feed("aerator-relay-status");

AdafruitIO_Feed *waterAlertFeed = io.feed("water-level-alert");
AdafruitIO_Feed *oxygenAlertFeed = io.feed("oxygen-alert");

// ===============================
// [3] Pin Setup
// ===============================

// Ultrasonic Sensor HC-SR04
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// Oxygen Sensor จำลองด้วย Potentiometer
const int OXYGEN_PIN = 34;

// Relay 2 ตัว
const int RELAY_PUMP = 25;      // Relay 1: ปั๊มน้ำ / ระบายน้ำ
const int RELAY_AERATOR = 33;   // Relay 2: เครื่องตีน้ำ / เพิ่มออกซิเจน

// LED
const int LED_G = 26;
const int LED_R = 27;

// ===============================
// [4] System Setting
// ===============================
const float POND_DEPTH = 200.0;

// ถ้าระยะผิวน้ำใกล้เซนเซอร์น้อยกว่า 50 cm = น้ำสูงเกิน
const float CRITICAL_DISTANCE = 50.0;

// ถ้าระดับน้ำต่ำกว่าหรือเท่ากับ 50 cm = น้ำน้อย
const float LOW_WATER_LEVEL = 50.0;

// ค่า Oxygen ขั้นต่ำ
const float MIN_OXYGEN = 5.0;   // mg/L

// ===============================
// [5] Timer
// ===============================
unsigned long lastUpdate = 0;
const unsigned long interval = 10000; // ส่งข้อมูลทุก 10 วินาที

// Relay states
int pumpRelayState = 0;
int aeratorRelayState = 0;

// ===============================
// Function Prototypes
// ===============================
void monitorAndSend();
float readUltrasonic(bool &error);
float readOxygen();
void setRelay(int relayPin, int state);
void setAlertLED(bool abnormal);

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(OXYGEN_PIN, INPUT);

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_AERATOR, OUTPUT);

  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);

  digitalWrite(RELAY_PUMP, LOW);
  digitalWrite(RELAY_AERATOR, LOW);

  setAlertLED(false);

  Serial.println("======================================");
  Serial.println(" WATER LEVEL + OXYGEN MONITOR STARTED ");
  Serial.println("======================================");

  Serial.print("Pond Depth: ");
  Serial.print(POND_DEPTH);
  Serial.println(" cm");

  Serial.print("Critical Distance: ");
  Serial.print(CRITICAL_DISTANCE);
  Serial.println(" cm");

  Serial.print("Low Water Level: ");
  Serial.print(LOW_WATER_LEVEL);
  Serial.println(" cm");

  Serial.print("Minimum Oxygen: ");
  Serial.print(MIN_OXYGEN);
  Serial.println(" mg/L");

  Serial.println("======================================");
  Serial.print("Connecting to Adafruit IO");

  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("[SUCCESS] Adafruit IO Connected!");
  Serial.println("======================================");

  monitorAndSend();
}

void loop() {
  io.run();

  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();
    monitorAndSend();
  }
}

// =====================================================
// อ่าน Sensor + ควบคุม Relay + ส่งข้อมูล
// =====================================================
void monitorAndSend() {
  bool ultrasonicError = false;

  // -------------------------------
  // 1. อ่าน Ultrasonic
  // -------------------------------
  float sensorDistance = readUltrasonic(ultrasonicError);

  float waterLevel = 0.0;

  if (!ultrasonicError) {
    waterLevel = POND_DEPTH - sensorDistance;
    waterLevel = constrain(waterLevel, 0, POND_DEPTH);
  }

  // -------------------------------
  // 2. อ่าน Oxygen Sensor
  // -------------------------------
  float oxygenLevel = readOxygen();

  // -------------------------------
  // 3. Water Alert Logic
  // -------------------------------
  String waterAlertMessage = "";
  bool waterHigh = false;
  bool waterLow = false;

  if (ultrasonicError) {
    waterAlertMessage = "Water Sensor Error!";
  }
  else if (sensorDistance < CRITICAL_DISTANCE) {
    waterHigh = true;
    waterAlertMessage = "Warning: High Water Level!";
  }
  else if (waterLevel <= LOW_WATER_LEVEL) {
    waterLow = true;
    waterAlertMessage = "Warning: Low Water Level!";
  }
  else {
    waterAlertMessage = "Normal Water Level!";
  }

  // -------------------------------
  // 4. Oxygen Alert Logic
  // -------------------------------
  String oxygenAlertMessage = "";
  bool oxygenLow = false;

  if (oxygenLevel < MIN_OXYGEN) {
    oxygenLow = true;
    oxygenAlertMessage = "Warning: Low Oxygen!";
  }
  else {
    oxygenAlertMessage = "Normal Oxygen!";
  }

  // -------------------------------
  // 5. Relay Logic
  // -------------------------------

  // Relay 1: ปั๊มน้ำ / ระบายน้ำ
  // เปิดเมื่อระดับน้ำสูงหรือต่ำผิดปกติ
  if (waterHigh || waterLow) {
    pumpRelayState = 1;
  } else {
    pumpRelayState = 0;
  }

  // Relay 2: เครื่องตีน้ำ / Aerator
  // เปิดเมื่อค่าออกซิเจนต่ำ
  if (oxygenLow) {
    aeratorRelayState = 1;
  } else {
    aeratorRelayState = 0;
  }

  setRelay(RELAY_PUMP, pumpRelayState);
  setRelay(RELAY_AERATOR, aeratorRelayState);

  // -------------------------------
  // 6. LED Alert Logic
  // -------------------------------
  // ถ้ามีเหตุการณ์ผิดปกติใด ๆ ให้ไฟแดงติด
  bool abnormal = ultrasonicError || waterHigh || waterLow || oxygenLow;

  setAlertLED(abnormal);

  // -------------------------------
  // 7. Serial Monitor
  // -------------------------------
  Serial.println();
  Serial.println(">>> Updating Cloud Data...");

  if (ultrasonicError) {
    Serial.println("Sensor Distance: ERROR");
    Serial.println("Water Level:     ERROR");
  } else {
    Serial.print("Sensor Distance: ");
    Serial.print(sensorDistance);
    Serial.println(" cm");

    Serial.print("Water Level:     ");
    Serial.print(waterLevel);
    Serial.println(" cm");
  }

  Serial.print("Oxygen Level:    ");
  Serial.print(oxygenLevel);
  Serial.println(" mg/L");

  Serial.print("Water Alert:     ");
  Serial.println(waterAlertMessage);

  Serial.print("Oxygen Alert:    ");
  Serial.println(oxygenAlertMessage);

  Serial.print("Pump Relay:      ");
  Serial.println(pumpRelayState == 1 ? "ON" : "OFF");

  Serial.print("Aerator Relay:   ");
  Serial.println(aeratorRelayState == 1 ? "ON" : "OFF");

  Serial.print("LED Status:      ");
  Serial.println(abnormal ? "RED - ABNORMAL" : "GREEN - NORMAL");

  // -------------------------------
  // 8. Send to Adafruit IO
  // -------------------------------
  if (!ultrasonicError) {
    levelFeed->save(waterLevel);
    Serial.print("Sent water-level: ");
    Serial.println(waterLevel);
  } else {
    Serial.println("Skip water-level because ultrasonic error.");
  }

  oxygenFeed->save(oxygenLevel);

  pumpRelayFeed->save(pumpRelayState);
  aeratorRelayFeed->save(aeratorRelayState);

  waterAlertFeed->save(waterAlertMessage);
  oxygenAlertFeed->save(oxygenAlertMessage);

  Serial.print("Sent oxygen-level: ");
  Serial.println(oxygenLevel);

  Serial.print("Sent pump-relay-status: ");
  Serial.println(pumpRelayState == 1 ? "ON" : "OFF");

  Serial.print("Sent aerator-relay-status: ");
  Serial.println(aeratorRelayState == 1 ? "ON" : "OFF");

  Serial.print("Sent water-level-alert: ");
  Serial.println(waterAlertMessage);

  Serial.print("Sent oxygen-alert: ");
  Serial.println(oxygenAlertMessage);

  Serial.println("Data sent to Adafruit IO.");
  Serial.println("---------------------------");
}

// =====================================================
// อ่านค่า Ultrasonic HC-SR04
// =====================================================
float readUltrasonic(bool &error) {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    error = true;
    return 999.0;
  }

  error = false;

  float distance = duration * 0.034 / 2.0;
  return distance;
}

// =====================================================
// อ่านค่า Oxygen Sensor
// ใช้ Potentiometer จำลองค่า Oxygen 0 - 14 mg/L
// =====================================================
float readOxygen() {
  int analogValue = analogRead(OXYGEN_PIN);

  // ESP32 analogRead ได้ค่า 0 - 4095
  // แปลงเป็น Oxygen 0.00 - 14.00 mg/L
  float oxygenMgL = map(analogValue, 0, 4095, 0, 1400) / 100.0;

  return oxygenMgL;
}

// =====================================================
// สั่ง Relay
// =====================================================
void setRelay(int relayPin, int state) {
  digitalWrite(relayPin, state == 1 ? HIGH : LOW);
}

// =====================================================
// LED Alert
// ถ้าผิดปกติ = LED แดง
// ถ้าปกติ = LED เขียว
// =====================================================
void setAlertLED(bool abnormal) {
  if (abnormal) {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  } else {
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  }
}