# ESP32-C3 MQTT Lights

This project is for a custom board I designed with an ESP32-C3. I originally planned on running ESPHOME on it, but support for the ESP32-C3 was still under development, so I wrote my own code instead.

With this code the ESP32 will connect to the wifi network, connect to the MQTT broker, and act as two MQTT lights. It then uses two PWM outputs using LEDC to switch on and off MOSFETs which control power to some 12V lights. These MQTT lights can then be controlled through Home Assistant

Since this is just a temporary project until the ESP32-C3 is fully supported in ESPHOME, I hard coded many of the values including the wifi SSID and password, the MQTT broker IP address, the MQTT topics, and the PWM interface. Ideally these would all be configurable from a web interface.

The KICAD files for the board this project is running on are located here:
https://github.com/CF209/kicad/tree/main/ESP32_12v_Relay

Instructions on setting up the ESP-IDF framework and the development environment are located here:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html

I used this serial adapter to program the ESP32-C3:
https://www.amazon.com/gp/product/B07BBPX8B8/
