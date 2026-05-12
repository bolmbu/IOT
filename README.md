# Fish Pond Water Level and Oxygen Monitoring System

โปรเจกต์นี้เป็นระบบจำลองการตรวจสอบระดับน้ำและค่าออกซิเจนในบ่อปลา โดยใช้ ESP32 ร่วมกับเซนเซอร์ Ultrasonic HC-SR04 และ Oxygen Sensor จำลองด้วย Potentiometer เพื่อควบคุม Relay 2 ตัว ได้แก่ Relay สำหรับปั๊มน้ำ และ Relay สำหรับเครื่องตีน้ำ / Aerator พร้อมส่งข้อมูลไปแสดงผลบน Adafruit IO Dashboard แบบ Real-time

## Features

- วัดระดับน้ำในบ่อด้วย Ultrasonic Sensor HC-SR04
- วัดค่าออกซิเจนในน้ำโดยใช้ Potentiometer จำลอง Oxygen Sensor
- ควบคุม Relay 2 ตัวแยกหน้าที่กัน
  - Relay 1 ใช้ควบคุมปั๊มน้ำ / ระบายน้ำ
  - Relay 2 ใช้ควบคุมเครื่องตีน้ำ / Aerator เพื่อเพิ่มออกซิเจน
- ถ้ามีเหตุการณ์ผิดปกติ ไฟ LED สีแดงจะติด
- ถ้าระบบปกติ ไฟ LED สีเขียวจะติด
- ส่งข้อมูลขึ้น Adafruit IO Dashboard
- แสดงข้อความแจ้งเตือนระดับน้ำและค่าออกซิเจนผ่าน Dashboard

## Required VS Code Extensions

1. PlatformIO IDE  
   ใช้สำหรับเขียน อัปโหลด และจัดการโปรเจกต์ ESP32 / Arduino

2. Wokwi Simulator  
   ใช้สำหรับจำลองการทำงานของวงจร ESP32 และเซนเซอร์ใน VS Code

3. C/C++ Extension Pack  
   ใช้ช่วยเขียนโค้ดภาษา C/C++ เช่น IntelliSense, ตรวจ syntax และ auto-complete

## Hardware / Components

- ESP32 DevKit
- HC-SR04 Ultrasonic Sensor
- Potentiometer ใช้จำลอง Oxygen Sensor
- Relay Module จำนวน 2 ตัว
- Green LED
- Red LED
- Jumper wires

## Pin Connection

| Component | ESP32 Pin |
|---|---|
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| Oxygen Sensor / Potentiometer SIG | GPIO 34 |
| Relay 1: Pump Relay | GPIO 25 |
| Relay 2: Aerator Relay | GPIO 33 |
| Green LED | GPIO 26 |
| Red LED | GPIO 27 |

## Adafruit IO Feeds

ต้องสร้าง Feed ใน Adafruit IO ตามชื่อนี้:

| Feed Name | Description |
|---|---|
| water-level | แสดงระดับน้ำในบ่อ หน่วย cm |
| oxygen-level | แสดงค่าออกซิเจนในน้ำ หน่วย mg/L |
| pump-relay-status | แสดงสถานะ Relay ปั๊มน้ำ 1 = ON, 0 = OFF |
| aerator-relay-status | แสดงสถานะ Relay เครื่องตีน้ำ 1 = ON, 0 = OFF |
| water-level-alert | แสดงข้อความแจ้งเตือนระดับน้ำ |
| oxygen-alert | แสดงข้อความแจ้งเตือนค่าออกซิเจน |

## Dashboard Setup

ใน Adafruit IO Dashboard แนะนำให้สร้าง Block ดังนี้:

| Dashboard Block | Feed |
|---|---|
| Gauge ระดับน้ำ | water-level |
| Gauge / Chart ค่าออกซิเจน | oxygen-level |
| Indicator สถานะปั๊มน้ำ | pump-relay-status |
| Indicator สถานะเครื่องตีน้ำ | aerator-relay-status |
| Text แจ้งเตือนระดับน้ำ | water-level-alert |
| Text แจ้งเตือนออกซิเจน | oxygen-alert |

## System Logic

ระบบใช้ HC-SR04 วัดระยะจากเซนเซอร์ถึงผิวน้ำ จากนั้นคำนวณระดับน้ำด้วยสูตร:

```text
Water Level = Pond Depth - Sensor Distance
