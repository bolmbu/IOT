#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "AdafruitIO_WiFi.h"

// ===============================
// [1] Adafruit IO & WiFi Setting
// ===============================
#define IO_USERNAME  "kanka"

// ใส่ Adafruit IO Key ของบัญชี kanka ตรงนี้
#define IO_KEY       "aio_zaaI10xZlZ2vf8Mh0NaRNCmMynNO"

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

// ระดับน้ำสำหรับเปิด/ปิดปั๊มอัตโนมัติ
const float PUMP_ON_LEVEL  = 150.0;  // น้ำสูงถึง 150 cm → Pump ON
const float PUMP_OFF_LEVEL = 50.0;  // น้ำลดถึง 120 cm → Pump OFF

// ช่วงอุณหภูมิปลอดภัย
const float MIN_SAFE_TEMP = 25.0;    // ต่ำกว่า 25°C = เย็นเกิน → Pump OFF
const float MAX_SAFE_TEMP = 32.0;    // สูงกว่า 32°C = ร้อนเกิน → Pump ON

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
  Serial.println(" FISH POND AUTO PUMP SYSTEM STARTED ");
  Serial.println("======================================");

  Serial.print("Pond Depth: ");
  Serial.print(POND_DEPTH);
  Serial.println(" cm");

  Serial.print("Pump ON Level: ");
  Serial.print(PUMP_ON_LEVEL);
  Serial.println(" cm");

  Serial.print("Pump OFF Level: ");
  Serial.print(PUMP_OFF_LEVEL);
  Serial.println(" cm");

  Serial.print("Safe Temp: ");
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
// อ่าน Sensor + ควบคุม Pump + ส่งข้อมูลไป Adafruit IO
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

  bool tempHot = false;
  bool tempCold = false;

  if (tempError) {
    tempStatus = "TEMP ERROR";
  } 
  else if (tempC > MAX_SAFE_TEMP) {
    tempStatus = "TOO HOT";
    tempHot = true;
  } 
  else if (tempC < MIN_SAFE_TEMP) {
    tempStatus = "TOO COLD";
    tempCold = true;
  }

  // -------------------------------
  // 5. Auto Pump Logic
  // -------------------------------
  String pumpReason = "";

  bool waterHigh = (!ultrasonicError && waterLevel >= PUMP_ON_LEVEL);
  bool waterLow  = (!ultrasonicError && waterLevel <= PUMP_OFF_LEVEL);

  if (ultrasonicError) {
    // ถ้าเซนเซอร์ระดับน้ำมีปัญหา ให้ปิดปั๊ม
    pumpState = 0;
    pumpReason = "Ultrasonic Error";
  }
  else if (waterHigh) {
    // น้ำสูงเกิน → เปิดปั๊ม
    pumpState = 1;
    pumpReason = "AUTO: Water Level High";
  }
  else if (tempHot) {
    // น้ำร้อนเกิน → เปิดปั๊มเพื่อหมุนเวียนน้ำ
    pumpState = 1;
    pumpReason = "AUTO: Water Too Hot";
  }
  else if (tempCold) {
    // น้ำเย็นเกิน → ปิดปั๊ม
    pumpState = 0;
    pumpReason = "AUTO: Water Too Cold";
  }
  else if (waterLow) {
    // น้ำต่ำ/ปกติ → ปิดปั๊ม
    pumpState = 0;
    pumpReason = "AUTO: Water Level Normal";
  }
  else {
    // อยู่ช่วงกลาง 120-150 cm → คงสถานะเดิม
    pumpReason = "AUTO: Keep Previous Pump State";
  }

  // สั่ง Relay + LED
  setPump(pumpState);

  // -------------------------------
  // 6. แสดงผล Serial Monitor
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

  Serial.print("Pump Reason:     ");
  Serial.println(pumpReason);

  Serial.print("Pump State:      ");
  Serial.println(pumpState == 1 ? "ON" : "OFF");

  // -------------------------------
  // 7. ส่งข้อมูลไป Adafruit IO
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
  pumpFeed->save(pumpState);

  Serial.print("Sent pump-status: ");
  Serial.println(pumpState == 1 ? "ON" : "OFF");

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
    // Pump ON
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
  } else {
    // Pump OFF
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  }
}