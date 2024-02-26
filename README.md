# Sensirion environmental sensor
Sensirion environment sensor project

Sensirion Environmental sensor integration using a esp32 for connecting to wifi, reading out data and sending it out on mqtt.

Simple enclosure to make it into a neat package.

![Enclosure](doc/finished.jpg)

This version uses a nodeMCU or wemos d1 mini esp3286 module.

## 
The code uses WifiManager to enable setting the wifi ssid and password to use so this does not have to be set compile time.
If there is no known wifi to connect to an access point (AP) is created and one can connect to that to set the wifi to connect to and the credentials to use.
This is done with http so not secure but is a one time deal.

The data is sent over MQTT, or can be fetched over http request the endpoint /data serves the sensordata in json format.

The MQTT server and settings is not configurable, TODO to fix that.

The Device also sends discovery message over MQTT in the format expected by home assistant so the sensor will be automatically added and discovered in the MQTT integration in home assistant.


# BOM:  
https://www.sensirion.com/products/catalog/SEN55  ~33$  
https://www.sparkfun.com/products/18079 ~1.5$   
https://en.wikipedia.org/wiki/NodeMCU or https://www.wemos.cc/en/latest/d1/d1_mini_3.1.0.html ~4$
Resistors for i2c pull-up somewhere between 2.2K to 10K should work.

Total BOM cost is ~40$
