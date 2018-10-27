# RESTful-Ride

Using ESP8266 to call RESTful API from http://TheRide.org to get real-time BUS data

[![RESTful-Ride](https://img.youtube.com/vi/-1dSZIs3ISw/0.jpg)](https://www.youtube.com/watch?v=-1dSZIs3ISw)

## Features:
* Easy to read interface
* Fast and Responsive
* Portable, i.e. small
* Wireless
* MQTT
* $$$ Cheap $$$

### Hardware
* NodeMCU (ESP8266)
* NeoPixel Ring (16 LEDs)

### Software
* Developer API from http://www.theride.org
* PlatformIO
* MQTT Server
* Home Assistant

### Wiring
Connect:
1. VCC of NeoPixel to 5V/Vin of NodeMCU
2. GND of NeoPIxel to GND of NodeMCU
3. Data in of NeoPIxel to RX pin of NodeMCU via 100-500Ohm resistor
