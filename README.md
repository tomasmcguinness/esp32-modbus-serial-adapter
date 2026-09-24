# ESP32 Modbus Adapter
The goal of this project is to provide a Modbus adapter with an onboard ESP32, allowing for a variety of uses. It is designed for the ESP32-C6 MCU, so can be used with ESPHome, ESP-IDF and Arduino.

# Hardware

The hardware folder contains a KiCad PCB design, designed around the ESP32-C6-MINI-1. Both the antenna and non-antenna versions will work here.

# SDM120M - Electrical Sensor

The first device supported by this project is the Eastron SDM120M Single Phase Energy Meter

The readings from this device will be exposed using as a Matter Electrical Sensor Device Type.
