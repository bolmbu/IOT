#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "AdafruitIO_WiFi.h"

// =====================================================
// [1] Adafruit IO & WiFi Setting
// =====================================================
// ชื่อบัญชีต้องตรงกับ Dashboard ในรูป: kanka
#define IO_USERNAME  "kanka"

// ใส่ Adafruit IO Key ของบัญชี kanka ตรงนี้
#define IO_KEY       "aio_zaaI10xZlZ2vf8Mh0NaRNCmMynNO"

#define WIFI_SSID    "Wokwi-GUEST"
#define WIFI_PASS    ""

// สร้างการเชื่อมต่อกับ Adafruit IO
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// =====================================================
// [2] Adafruit IO Feeds
// =====================================================
// ใช้แสดงค่าระดับน้ำ
AdafruitIO_Feed *levelFeed = io.feed("water-level");

// ใช้แสดงค่าอุณหภูมิ
AdafruitIO_Feed *tempFeed = io.feed("water-temp");

// ใช้รับคำสั่งจากปุ่ม Dashboard
AdafruitIO_Feed *pumpControlFeed = io.feed("pump-control");

// ใช้ส่งสถานะปั๊มจริงกลับไป Dashboard
AdafruitIO_Feed *pumpStatusFeed = io.feed("pump-status");

// =====================================================
// [3] Pin Setup
// =====================================================
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int TEMP_PIN = 4;

const int RELAY_PIN = 25;
const int LED_G = 26;
const int LED_R = 27;

// =====================================================
// [4] System Settings
// =====================================================
const float POND_DEPTH = 200.0;          // ความลึกบ่อ 200 cm
const float SAFE_ZONE_PERCENT = 0.25;    // 25% ของความลึกบ่อ
const float MIN_SAFE_TEMP = 25.0;
const float MAX_SAFE_TEMP = 32.0;

const float CRITICAL_DISTANCE = POND_DEPTH * SAFE_ZONE_PERCENT;

// ส่งข้อมูลทุก 10 วินาที
unsigned long lastUpdate = 0;
const unsigned long interval = 10000;

// โหมดควบคุมปั๊ม
// false = ระบบควบคุมอัตโนมัติ
// true  = ผู้ใช้กดจาก Dashboard
bool manualMode = false;
int manualPumpState = LOW;

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// Function Prototype
void handlePumpDashboard(AdafruitIO_Data *data);
void monitorAndSend();
float readUltrasonicDistance(bool &error);
float readTemperature(bool &error);
void setPump(int state);
void sendToAdafruit(float waterLevel, float tempC, bool ultrasonicError, bool tempError, int pumpState);

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);

  setPump(LOW);

  sensors.begin();

  Serial.println("======================================");
  Serial.println("FISH POND MONITORING SYSTEM");
  Serial.print("Adafruit Username: ");
  Serial.println(IO_USERNAME);
  Serial.print("Critical Distance: ");
  Serial.print(CRITICAL_DISTANCE);
  Serial.println(" cm");
  Serial.println("======================================");

  // รับคำสั่งจาก Dashboard ผ่าน feed pump-control
  pumpControlFeed->onMessage(handlePumpDashboard);

  // เชื่อมต่อ Adafruit IO
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("[CONNECTED] Adafruit IO Connected!");
  Serial.println("======================================");

  // อ่านค่าล่าสุดจาก feed pump-control
  pumpControlFeed->get();

  // ส่งข้อมูลทันที 1 ครั้งหลังเชื่อมต่อ
  monitorAndSend();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  io.run();

  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();
    monitorAndSend();
  }
}

// =====================================================
// รับคำสั่งจาก Dashboard
// =====================================================
// ใน Dashboard ให้สร้าง Toggle/Button ผูกกับ feed: pump-control
// ค่า ON = 1, OFF = 0
void handlePumpDashboard(AdafruitIO_Data *data) {
  int status = data->toPinLevel();

  manualMode = true;
  manualPumpState = status;

  setPump(manualPumpState);

  Serial.println();
  Serial.println("---------------------------");
  Serial.print("COMMAND FROM DASHBOARD: ");
  Serial.println(status == HIGH ? "PUMP ON" : "PUMP OFF");
  Serial.println("Manual Mode: ON");
  Serial.println("---------------------------");

  // ส่งสถานะจริงกลับไปที่ pump-status
  pumpStatusFeed->save(status == HIGH ? 1 : 0);
}

// =====================================================
// อ่านเซนเซอร์ + คำนวณ + ส่งข้อมูล
// =====================================================
void monitorAndSend() {
  bool ultrasonicError = false;
  bool tempError = false;

  // 1. อ่านระยะจาก Ultrasonic
  float sensorDistance = readUltrasonicDistance(ultrasonicError);

  // 2. คำนวณระดับน้ำ
  float waterLevel = 0.0;

  if (!ultrasonicError) {
    waterLevel = POND_DEPTH - sensorDistance;

    if (waterLevel < 0) {
      waterLevel = 0;
    }

    if (waterLevel > POND_DEPTH) {
      waterLevel = POND_DEPTH;
    }
  }

  // 3. อ่านอุณหภูมิ
  float tempC = readTemperature(tempError);

  // 4. ตรวจสอบสถานะระบบ
  bool warning = false;
  String reason = "";

  if (ultrasonicError) {
    warning = true;
    reason += "Ultrasonic Error ";
  }

  if (tempError) {
    warning = true;
    reason += "Temperature Sensor Error ";
  }

  if (!ultrasonicError && sensorDistance <= CRITICAL_DISTANCE) {
    warning = true;
    reason += "Water Level Too High ";
  }

  if (!tempError && tempC > MAX_SAFE_TEMP) {
    warning = true;
    reason += "Water Too Hot ";
  }

  if (!tempError && tempC < MIN_SAFE_TEMP) {
    warning = true;
    reason += "Water Too Cold ";
  }

  // 5. ควบคุมปั๊ม
  int pumpState = LOW;

  if (manualMode) {
    // ถ้ากดจาก Dashboard ให้ใช้ค่าจาก Dashboard
    pumpState = manualPumpState;
  } else {
    // ถ้าไม่ได้กดเอง ให้ระบบควบคุมอัตโนมัติ
    pumpState = warning ? HIGH : LOW;
  }

  setPump(pumpState);

  // 6. แสดงผลใน Serial Monitor
  Serial.println();
  Serial.println("--- Status Report ---");

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

  if (tempError) {
    Serial.println("Temperature:     ERROR");
  } else {
    Serial.print("Temperature:     ");
    Serial.print(tempC);
    Serial.println(" C");
  }

  Serial.print("System Status:   ");
  Serial.println(warning ? "WARNING" : "NORMAL");

  Serial.print("Reason:          ");
  if (reason == "") {
    Serial.println("None");
  } else {
    Serial.println(reason);
  }

  Serial.print("Control Mode:    ");
  Serial.println(manualMode ? "MANUAL" : "AUTO");

  Serial.print("Pump State:      ");
  Serial.println(pumpState == HIGH ? "ON" : "OFF");

  // 7. ส่งข้อมูลไป Adafruit IO
  sendToAdafruit(waterLevel, tempC, ultrasonicError, tempError, pumpState);
}

// =====================================================
// อ่านค่า Ultrasonic HC-SR04
// =====================================================
float readUltrasonicDistance(bool &error) {
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
// อ่านค่าอุณหภูมิ DS18B20
// =====================================================
float readTemperature(bool &error) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C || tempC <= -100) {
    error = true;
    return 0.0;
  }

  error = false;
  return tempC;
}

// =====================================================
// สั่ง Relay + LED
// =====================================================
void setPump(int state) {
  digitalWrite(RELAY_PIN, state);

  if (state == HIGH) {
    // ปั๊มทำงาน
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  } else {
    // ปั๊มหยุด
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  }
}

// =====================================================
// ส่งข้อมูลไป Adafruit IO
// =====================================================
void sendToAdafruit(float waterLevel, float tempC, bool ultrasonicError, bool tempError, int pumpState) {
  if (!ultrasonicError) {
    levelFeed->save(waterLevel);
    Serial.println("Sent water-level to Adafruit IO.");
  } else {
    Serial.println("Skip water-level because ultrasonic error.");
  }

  if (!tempError) {
    tempFeed->save(tempC);
    Serial.println("Sent water-temp to Adafruit IO.");
  } else {
    Serial.println("Skip water-temp because temperature error.");
  }

  pumpStatusFeed->save(pumpState == HIGH ? 1 : 0);
  Serial.println("Sent pump-status to Adafruit IO.");

  Serial.println("Data sent to Adafruit IO.");
}