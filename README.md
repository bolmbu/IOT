# Fish Pond Auto Pump Monitoring System

โปรเจกต์นี้เป็นระบบจำลองการตรวจสอบระดับน้ำและอุณหภูมิของบ่อปลา โดยใช้ ESP32 ร่วมกับเซนเซอร์ Ultrasonic HC-SR04 และ DS18B20 เพื่อควบคุมปั๊มน้ำอัตโนมัติ พร้อมส่งข้อมูลไปแสดงผลบน Adafruit IO Dashboard

## Features

- วัดระดับน้ำในบ่อด้วย Ultrasonic Sensor
- วัดอุณหภูมิน้ำด้วย DS18B20
- ควบคุมปั๊มน้ำอัตโนมัติตามระดับน้ำและอุณหภูมิ
- ส่งข้อมูลขึ้น Adafruit IO Dashboard
- แสดงสถานะปั๊มผ่าน Feed: pump-status
- แสดงสถานะผ่าน LED เขียว/แดง

## Required VS Code Extensions

1. PlatformIO IDE  
   ใช้สำหรับเขียน อัปโหลด และจัดการโปรเจกต์ ESP32/Arduino

2. Wokwi Simulator  
   ใช้สำหรับจำลองการทำงานของวงจร ESP32 และเซนเซอร์ใน VS Code

3. C/C++ Extension Pack  
   ใช้ช่วยเขียนโค้ดภาษา C/C++ เช่น IntelliSense, ตรวจ syntax และ auto-complete

## Hardware / Components

- ESP32 DevKit
- HC-SR04 Ultrasonic Sensor
- DS18B20 Temperature Sensor
- Relay Module
- Green LED
- Red LED
- Resistors
- Jumper wires

## Pin Connection

| Component | ESP32 Pin |
|---|---|
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| DS18B20 DATA | GPIO 4 |
| Relay | GPIO 25 |
| Green LED | GPIO 26 |
| Red LED | GPIO 27 |

## Adafruit IO Feeds

ต้องสร้าง Feed ใน Adafruit IO ตามชื่อนี้:

| Feed Name | Description |
|---|---|
| water-level | แสดงระดับน้ำในบ่อ หน่วย cm |
| water-temp | แสดงอุณหภูมิน้ำ หน่วย °C |
| pump-status | แสดงสถานะปั๊ม 1 = ON, 0 = OFF |

## Auto Pump Logic

ระบบควบคุมปั๊มอัตโนมัติตามเงื่อนไขนี้:

| Condition | Pump Status |
|---|---|
| Water Level >= 150 cm | ON |
| Water Level <= 50 cm | OFF |
| Temperature > 32°C | ON |
| Temperature < 25°C | OFF |
| Sensor Error | OFF |

หมายเหตุ: ระดับน้ำคำนวณจากสูตร  
Water Level = Pond Depth - Sensor Distance

โดยในโค้ดกำหนด Pond Depth = 200 cm

## Important Settings in Code

```cpp
const float POND_DEPTH = 200.0;
const float PUMP_ON_LEVEL  = 150.0;
const float PUMP_OFF_LEVEL = 50.0;
const float MIN_SAFE_TEMP = 25.0;
const float MAX_SAFE_TEMP = 32.0;
