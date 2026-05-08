# Fish Pond Auto Pump Monitoring System

โปรเจกต์นี้เป็นระบบจำลองการตรวจสอบระดับน้ำและอุณหภูมิของบ่อปลา โดยใช้ ESP32 ร่วมกับเซนเซอร์ Ultrasonic HC-SR04 และ DS18B20 เพื่อควบคุม Relay / Pump อัตโนมัติ พร้อมส่งข้อมูลไปแสดงผลบน Adafruit IO Dashboard แบบ Real-time

## Features

- วัดระดับน้ำในบ่อด้วย Ultrasonic Sensor HC-SR04
- วัดอุณหภูมิน้ำด้วย DS18B20 Temperature Sensor
- ควบคุม Relay / Pump อัตโนมัติตามระดับน้ำและอุณหภูมิ
- แสดงสถานะด้วย LED สีเขียวและ LED สีแดง
- ส่งข้อมูลขึ้น Adafruit IO Dashboard
- ส่งข้อความแจ้งเตือนระดับน้ำ เช่น น้ำปกติ, น้ำเยอะ, น้ำน้อย
- ส่งข้อความแจ้งเตือนอุณหภูมิ เช่น อุณหภูมิปกติ, ต่ำเกิน, สูงเกิน

## Required VS Code Extensions

1. **PlatformIO IDE**  
   ใช้สำหรับเขียน อัปโหลด และจัดการโปรเจกต์ ESP32 / Arduino

2. **Wokwi Simulator**  
   ใช้สำหรับจำลองการทำงานของวงจร ESP32 และเซนเซอร์ใน VS Code

3. **C/C++ Extension Pack**  
   ใช้ช่วยเขียนโค้ดภาษา C/C++ เช่น IntelliSense, ตรวจ syntax และ auto-complete

## Hardware / Components

- ESP32 DevKit
- HC-SR04 Ultrasonic Sensor
- DS18B20 Temperature Sensor
- Relay Module
- Green LED
- Red LED
- Resistor
- Jumper wires

## Pin Connection

| Component | ESP32 Pin |
|---|---|
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| DS18B20 DATA | GPIO 4 |
| Relay Module | GPIO 25 |
| Green LED | GPIO 26 |
| Red LED | GPIO 27 |

## Adafruit IO Feeds

ต้องสร้าง Feed ใน Adafruit IO ตามชื่อนี้:

| Feed Name | Description |
|---|---|
| water-level | แสดงระดับน้ำในบ่อ หน่วย cm |
| water-temp | แสดงอุณหภูมิน้ำ หน่วย °C |
| pump-status | แสดงสถานะ Relay / Pump โดย 1 = ON และ 0 = OFF |
| water-level-alert | แสดงข้อความแจ้งเตือนระดับน้ำ |
| temp-alert | แสดงข้อความแจ้งเตือนอุณหภูมิ |

## Dashboard Setup

ใน Adafruit IO Dashboard แนะนำให้สร้าง Block ดังนี้:

| Dashboard Block | Feed |
|---|---|
| Gauge ระดับน้ำ | water-level |
| Gauge อุณหภูมิ | water-temp |
| Indicator / Text สถานะ Pump | pump-status |
| Text แจ้งเตือนระดับน้ำ | water-level-alert |
| Text แจ้งเตือนอุณหภูมิ | temp-alert |
| Line Chart | water-temp หรือ water-level |

## System Logic

ระบบใช้ HC-SR04 วัดระยะจากเซนเซอร์ถึงผิวน้ำ จากนั้นคำนวณระดับน้ำด้วยสูตร:

```text
Water Level = Pond Depth - Sensor Distance
