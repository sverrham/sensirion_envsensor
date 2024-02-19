# README

The current json library arduino-libraries/ArduinoMqttClient@^0.1.7 does not support larger buffers than 256 characters.

this needs to be update in MqttClient.cpp to 512:
#define TX_PAYLOAD_BUFFER_SIZE 256

