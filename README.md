# ESP32 Modbus Adapter
The goal of this project is to provide a Modbus (RS-485) adapter with an onboard ESP32, allowing for a variety of uses. Designed around the ESP32-C6 MCU, so can be used with ESPHome, ESP-IDF and Arduino. The radio supports WiFi, Thread and Zigbee, giving you lots of flexibility. 

# Hardware

The hardware folder contains a KiCad PCB design, designed around the ESP32-C6-MINI-1. Both the antenna and non-antenna (U1) versions will work here due to the location of the ESP32.

<img width="994" height="664" alt="image" src="https://github.com/user-attachments/assets/64eba7b2-16b6-4c7b-80d4-a42a3e53cc8d" />

My PCB design also includes some debugging headers, allowing access to UART0 for debugging and flashing.

# Examples

At present, there are three Matter based example available. I have these three devices at home and I use my boards to get information from them!

## Matter

As I'm a big fan of Matter, I've been using that exclusively. All of these firmware examples just read data from the device. I hope to expand support in the future to allow some control, where it makes sense.

### Eastern SDM120M 

The first device supported by this project is the Eastron SDM120M Single Phase Energy Meter. The readings from this device will be exposed using as a Matter Electrical Sensor Device Type.

### Solax X1-G4 Solar Inverter

This exposes the various numbers, like power, PV string power and battery state over the Matter Solar Power Device Type. It can be connected to the CAD port of the inverter.

<img width="1370" height="912" alt="image" src="https://github.com/user-attachments/assets/a874894d-0c9c-4fda-89a8-bfab00cdb13d" />

I adapted an ethernet patch cable to give me the right A/B terminal connections. I use the data from this device in my Home Energy Manager project.

<img width="391" height="284" alt="image" src="https://github.com/user-attachments/assets/c5382b75-0860-4346-9f83-0a5aa51566c5" />

### Kronoterm Adapt 0312 Heat Pump

Exposes readings like flow and return temperatures in addition to a read only thermostat over the Heat Pump Device Type. 

It also reports on hot water status. I'm using it in my Matter powered Heating Monitor project.

<img width="1013" height="353" alt="image" src="https://github.com/user-attachments/assets/c95cb5a3-31ae-46ff-9808-e3d10a7424a8" />



