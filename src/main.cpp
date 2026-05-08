#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "AdafruitIO_WiFi.h"

// ===============================
// [1] Adafruit IO & WiFi Setting
// ===============================
#define IO_USERNAME  "kanka"

// ใส่ Adafruit IO Key ของบัญชี kanka ตรงนี้
#define IO_KEY       "Adafuirt_key"

#define WIFI_SSID    "Wokwi-GUEST"
#define WIFI_PASS    ""

// สร้างการเชื่อมต่อกับ Adafruit IO
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ===============================
// [2] Adafruit IO Feeds
// ===============================
AdafruitIO_Feed *levelFeed = io.feed("water-level");
AdafruitIO_Feed *tempFeed  = io.feed("water-temp");
AdafruitIO_Feed *pumpFeed  = io.feed("pump-status");

// Feed สำหรับข้อความแจ้งเตือน
AdafruitIO_Feed *waterAlertFeed = io.feed("water-level-alert");
AdafruitIO_Feed *tempAlertFeed  = io.feed("temp-alert");

// ===============================
// [3] Pin Setup
// ===============================
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int TEMP_PIN = 4;

const int RELAY_PIN = 25;
const int LED_G = 26;
const int LED_R = 27;

// ===============================
// [4] System Setting
// ===============================
const float POND_DEPTH = 200.0;

// ระยะห่างผิวน้ำ < 50 cm = น้ำสูง/น้ำล้นวิกฤต
// ระยะห่างผิวน้ำ > 50 cm = ระดับน้ำปกติ ถ้าอุณหภูมิปกติด้วย
const float CRITICAL_DISTANCE = 50.0;

// ใช้สำหรับแจ้งเตือนน้ำน้อย
// ถ้า Water Level <= 50 cm = น้ำน้อย
const float LOW_WATER_LEVEL = 50.0;

// ช่วงอุณหภูมิปกติ 25 - 32°C
const float MIN_SAFE_TEMP = 25.0;
const float MAX_SAFE_TEMP = 32.0;

// ===============================
// [5] Sensor Setup
// ===============================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ===============================
// [6] Timer
// ===============================
unsigned long lastUpdate = 0;
const unsigned long interval = 10000; // ส่งข้อมูลทุก 10 วินาที

// สถานะปั๊ม
int pumpState = 0;

// ===============================
// Function Prototypes
// ===============================
void monitorAndSend();
float readUltrasonic(bool &error);
float readTemperature(bool &error);
void setPump(int state);

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
  Serial.println(" WATER LEVEL TELEMETRY SYSTEM STARTED ");
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

  Serial.print("Normal Temp Range: ");
  Serial.print(MIN_SAFE_TEMP);
  Serial.print(" - ");
  Serial.print(MAX_SAFE_TEMP);
  Serial.println(" C");

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

  // ส่งข้อมูลครั้งแรกทันที
  monitorAndSend();
}

void loop() {
  // ต้องมีเสมอ เพื่อให้ Adafruit IO ทำงานต่อเนื่อง
  io.run();

  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();
    monitorAndSend();
  }
}

// =====================================================
// อ่าน Sensor + Logic ตามตาราง + ส่งข้อมูลไป Adafruit IO
// =====================================================
void monitorAndSend() {
  bool ultrasonicError = false;
  bool tempError = false;

  // -------------------------------
  // 1. อ่านค่า Ultrasonic
  // -------------------------------
  float sensorDistance = readUltrasonic(ultrasonicError);

  // -------------------------------
  // 2. คำนวณระดับน้ำ
  // Water Level = Pond Depth - Sensor Distance
  // -------------------------------
  float waterLevel = 0.0;

  if (!ultrasonicError) {
    waterLevel = POND_DEPTH - sensorDistance;
    waterLevel = constrain(waterLevel, 0, POND_DEPTH);
  }

  // -------------------------------
  // 3. อ่านอุณหภูมิ
  // -------------------------------
  float tempC = readTemperature(tempError);

  // -------------------------------
  // 4. เช็กสถานะอุณหภูมิ
  // -------------------------------
  String tempStatus = "NORMAL";

  bool tempNormal = false;
  bool tempAbnormal = false;

  if (tempError) {
    tempStatus = "TEMP ERROR";
  }
  else if (tempC > MAX_SAFE_TEMP) {
    tempStatus = "TOO HOT";
    tempAbnormal = true;
  }
  else if (tempC < MIN_SAFE_TEMP) {
    tempStatus = "TOO COLD";
    tempAbnormal = true;
  }
  else {
    tempStatus = "NORMAL";
    tempNormal = true;
  }

  // -------------------------------
  // 5. สร้างข้อความ Alert
  // -------------------------------
  String waterAlertMessage = "";
  String tempAlertMessage = "";

  // แจ้งเตือนอุณหภูมิ
  if (tempError) {
    tempAlertMessage = "Temperature Sensor Error!";
  }
  else if (tempC < MIN_SAFE_TEMP) {
    tempAlertMessage = "Warning: Low Temp!";
  }
  else if (tempC > MAX_SAFE_TEMP) {
    tempAlertMessage = "Warning: High Temp!";
  }
  else {
    tempAlertMessage = "Normal Temp!";
  }

  // แจ้งเตือนระดับน้ำ
  if (ultrasonicError) {
    waterAlertMessage = "Water Sensor Error!";
  }
  else if (sensorDistance < CRITICAL_DISTANCE) {
    // ผิวน้ำอยู่ใกล้เซนเซอร์มาก = น้ำสูง / น้ำล้น
    waterAlertMessage = "Warning: High Water Level!";
  }
  else if (waterLevel <= LOW_WATER_LEVEL) {
    // ระดับน้ำต่ำเกินไป
    waterAlertMessage = "Warning: Low Water Level!";
  }
  else {
    waterAlertMessage = "Normal Water Level!";
  }

  // -------------------------------
  // 6. Auto Logic ตามตารางทดสอบ
  // -------------------------------
  String systemStatus = "";
  String pumpReason = "";

  bool waterCritical = (!ultrasonicError && sensorDistance < CRITICAL_DISTANCE);
  bool waterNormal   = (!ultrasonicError && sensorDistance > CRITICAL_DISTANCE);

  if (ultrasonicError) {
    // ถ้า Ultrasonic Error ให้ปิดปั๊มเพื่อความปลอดภัย
    pumpState = 0;
    systemStatus = "ERROR";
    pumpReason = "Ultrasonic Error";
  }
  else if (tempError) {
    // ถ้า Temp Error ให้ปิดปั๊มเพื่อความปลอดภัย
    pumpState = 0;
    systemStatus = "ERROR";
    pumpReason = "Temperature Sensor Error";
  }
  else if (waterCritical) {
    // ระยะห่างผิวน้ำ < 50 cm
    // น้ำล้นวิกฤต → Relay ON / LED แดงติด
    pumpState = 1;
    systemStatus = "WARNING";
    pumpReason = "Water Critical: Distance < 50 cm";
  }
  else if (tempAbnormal) {
    // อุณหภูมิ > 32°C หรือ < 25°C
    // Relay ON / LED แดงติด
    pumpState = 1;
    systemStatus = "WARNING";
    pumpReason = "Temperature Abnormal";
  }
  else if (waterNormal && tempNormal) {
    // ระยะห่างผิวน้ำ > 50 cm และอุณหภูมิ 25–32°C
    // Relay OFF / LED เขียวติด
    pumpState = 0;
    systemStatus = "NORMAL";
    pumpReason = "Normal Condition";
  }
  else {
    // กรณีระยะเท่ากับ 50 cm พอดี หรือเคสอื่น ๆ
    pumpState = 0;
    systemStatus = "NORMAL";
    pumpReason = "Default Normal";
  }

  // สั่ง Relay + LED
  setPump(pumpState);

  // -------------------------------
  // 7. แสดงผล Serial Monitor
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

  if (tempError) {
    Serial.println("Temperature:     ERROR");
  } else {
    Serial.print("Temperature:     ");
    Serial.print(tempC);
    Serial.println(" C");
  }

  Serial.print("Temp Status:     ");
  Serial.println(tempStatus);

  Serial.print("System Status:   ");
  Serial.println(systemStatus);

  Serial.print("Pump Reason:     ");
  Serial.println(pumpReason);

  Serial.print("Pump State:      ");
  Serial.println(pumpState == 1 ? "ON" : "OFF");

  Serial.print("Water Alert:     ");
  Serial.println(waterAlertMessage);

  Serial.print("Temp Alert:      ");
  Serial.println(tempAlertMessage);

  // -------------------------------
  // 8. ส่งข้อมูลไป Adafruit IO
  // -------------------------------
  if (!ultrasonicError) {
    levelFeed->save(waterLevel);
    Serial.print("Sent water-level: ");
    Serial.println(waterLevel);
  } else {
    Serial.println("Skip water-level because ultrasonic error.");
  }

  if (!tempError) {
    tempFeed->save(tempC);
    Serial.print("Sent water-temp: ");
    Serial.println(tempC);
  } else {
    Serial.println("Skip water-temp because temperature error.");
  }

  // ส่ง Pump Status ไป Adafruit IO
  // 1 = ON, 0 = OFF
  pumpFeed->save(pumpState);

  // ส่งข้อความแจ้งเตือนไป Adafruit IO
  waterAlertFeed->save(waterAlertMessage);
  tempAlertFeed->save(tempAlertMessage);

  Serial.print("Sent pump-status: ");
  Serial.println(pumpState == 1 ? "ON" : "OFF");

  Serial.print("Sent water-level-alert: ");
  Serial.println(waterAlertMessage);

  Serial.print("Sent temp-alert: ");
  Serial.println(tempAlertMessage);

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
// อ่านค่าอุณหภูมิ DS18B20
// =====================================================
float readTemperature(bool &error) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
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
    // Relay ON / Pump ON / LED แดงติด
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  } else {
    // Relay OFF / Pump OFF / LED เขียวติด
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  }
}
