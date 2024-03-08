
/*
 * Copyright (c) 2021, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Arduino.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>

#include <PubSubClient.h>
#include <ESPAsyncWiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <Preferences.h>

#include "ha_discovery.h"

Preferences prefs;

AsyncWebServer server(80);
DNSServer dns;


SensirionI2CSen5x sen5x;
unsigned char serialNumber[32];
String hw_version;
String sw_version;
JsonDocument sensordata;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// This is the topic this program will send the state of this device to.
String stateTopic;
String mqttServerIp;
int mqttServerPort;
boolean mqttEnabled;


void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

const char* TOPIC_MESSAGE = "Mqtt topic";
const char* SERVER_IP_MESSAGE = "Mqtt Server";
const char* SERVER_PORT_MESSAGE = "Mqtt port";
const char* MQTT_ENABLED_MESSAGE = "mqtt_enabled";

String genHtml(String stateTopic, String mqttServerIp, String mqttServerPort, bool mqttEnabled) {
    String html;
    html += R"(<!DOCTYPE HTML><html><head>)";
    html += R"(<title>Environmental Sensor</title>)";
    html += R"(<meta name="viewport" content="width=device-width, initial-scale=1">)";
    html += R"(</head><body>)";
    html += R"(<form action="/get">)";
    html += R"(<input type="text" name="Mqtt topic" value=")" + stateTopic + R"(">)";
    html += R"(<label for="Mqtt topic"> MQTT Topic</label><br>)";
    html += R"(<input type="text" name="Mqtt Server" value=")" + mqttServerIp + R"(">)";
    html += R"(<label for="Mqtt Server"> MQTT Server IP</label><br>)";
    html += R"(<input type="text" name="Mqtt port" value=")" + mqttServerPort + R"(">)";
    html += R"(<label for="Mqtt port"> MQTT Server Port</label><br>)";
    // html += R"(<input type='hidden' value='No' name='mqtt_enabled'>)"; //Hiden value to send No if the checkbox is not sent.
    if (mqttEnabled) {
        html += R"(<input type="checkbox" name="mqtt_enabled" value="Yes" checked>)";
    } else {
        html += R"(<input type="checkbox" name="mqtt_enabled" value="Yes">)";
    }
    html += R"(<label for="mqtt_enabled"> MQTT Enabled</label><br><br>)";
    html += R"(<input type="submit" value="Submit">)";
    html += R"(</form><br>)";
    html += "<h1 class=\"label\">MQTT Config</h1>";
    html += "MQTT Topic: " + stateTopic + "<br>";
    html += "MQTT Server: " + mqttServerIp + "<br>";
    html += "MQTT Port: " + mqttServerPort + "<br>";
    String enabled = mqttEnabled ? "Yes" : "No";
    html += "MQTT Enabled: " + enabled + "<br>";
    html += R"(<p><a href="/data">Json sensor data</a></p><br>)";
    html += "</body></html>";

    return html;
}


// The used commands use up to 48 bytes. On some Arduino's the default buffer
// space is not large enough
#define MAXBUF_REQUIREMENT 48

#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif


void printModuleVersions() {
    uint16_t error;
    char errorMessage[256];

    unsigned char productName[32];
    uint8_t productNameSize = 32;

    error = sen5x.getProductName(productName, productNameSize);

    if (error) {
        Serial.print("Error trying to execute getProductName(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("ProductName:");
        Serial.println((char*)productName);
    }

    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    bool firmwareDebug;
    uint8_t hardwareMajor;
    uint8_t hardwareMinor;
    uint8_t protocolMajor;
    uint8_t protocolMinor;

    error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                             hardwareMajor, hardwareMinor, protocolMajor,
                             protocolMinor);
    if (error) {
        Serial.print("Error trying to execute getVersion(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Firmware: ");
        Serial.print(firmwareMajor);
        Serial.print(".");
        Serial.print(firmwareMinor);
        Serial.print(", ");

        sw_version = String(firmwareMajor) + "." + String(firmwareMinor);

        Serial.print("Hardware: ");
        Serial.print(hardwareMajor);
        Serial.print(".");
        Serial.println(hardwareMinor);
        
        hw_version = String(hardwareMajor) + "." + String(hardwareMinor);
    }
}


void printSerialNumber() {
    uint16_t error;
    char errorMessage[256];
    // unsigned char serialNumber[32];
    uint8_t serialNumberSize = 32;

    error = sen5x.getSerialNumber(serialNumber, serialNumberSize);
    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("SerialNumber:");
        Serial.println((char*)serialNumber);
    }
}


struct SensirionMeasurement
{
    float massConcentrationPm1p0;
    float massConcentrationPm2p5;
    float massConcentrationPm4p0;
    float massConcentrationPm10p0;
    float ambientHumidity;
    float ambientTemperature;
    float vocIndex;
    float noxIndex;
};


void publishMQTT(JsonDocument doc, String topic_dev) {
    // Serialize the JSON document to a char buffer
    char jsonBuffer[512];
    size_t n = serializeJson(doc, jsonBuffer);
    const char* payload = jsonBuffer;
    mqttClient.publish(topic_dev.c_str(), jsonBuffer, n);
    Serial.println("Published message: "+ topic_dev + String(payload));
}

void sendMQTT(SensirionMeasurement data) {
    JsonDocument doc;
  
    // Send the sensirion data, to environment/sensirion/technical/
    // String topic_dev = "environment/sensirion/garage";

    doc["pm1p0"] = data.massConcentrationPm1p0;
    doc["pm2p5"] = data.massConcentrationPm2p5;
    doc["pm4p0"] = data.massConcentrationPm4p0;
    doc["pm10p0"] = data.massConcentrationPm10p0;
    doc["humidity"] = data.ambientHumidity;
    doc["temperature"] = data.ambientTemperature;
    doc["vocIndex"] = data.vocIndex;
    doc["noxIndex"] = data.noxIndex;

    publishMQTT(doc, stateTopic);
    
    sensordata = doc;
}

void reconnectMQTT() {
    String mac_addr = WiFi.macAddress();

    if (!mqttClient.connected()) {
        mqttClient.setServer(mqttServerIp.c_str(), mqttServerPort);
        while (!mqttClient.connect(mac_addr.c_str())) {
            Serial.println("MQTT Not Connected");
            delay (1000);
        }
        // Send discovery messages
        HaDiscovery ha_discovery(stateTopic);
        ha_discovery.setDeviceInfo(String((char*) serialNumber), hw_version, sw_version);
        publishMQTT(ha_discovery.getMQTTPm1p0DiscoveryMsg(), ha_discovery.getDiscoveryTopicPm1p0());
        publishMQTT(ha_discovery.getMQTTPm2p5DiscoveryMsg(), ha_discovery.getDiscoveryTopicPm2p5());
        publishMQTT(ha_discovery.getMQTTPm4p0DiscoveryMsg(), ha_discovery.getDiscoveryTopicPm4p0());
        publishMQTT(ha_discovery.getMQTTPm10p0DiscoveryMsg(), ha_discovery.getDiscoveryTopicPm10p0());
        publishMQTT(ha_discovery.getMQTTTemperatureDiscoveryMsg(), ha_discovery.getDiscoveryTopicTemperature());
        publishMQTT(ha_discovery.getMQTTHumidityDiscoveryMsg(), ha_discovery.getDiscoveryTopicHumidity());
        publishMQTT(ha_discovery.getMQTTVocIndexDiscoveryMsg(), ha_discovery.getDiscoveryTopicVocindex());
        publishMQTT(ha_discovery.getMQTTNoxIndexDiscoveryMsg(), ha_discovery.getDiscoveryTopicNoxindex());

    }
    Serial.println("MQTT Connected");
}

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }

    Wire.begin();

    sen5x.begin(Wire);

    uint16_t error;
    char errorMessage[256];
    error = sen5x.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

// Print SEN55 module information if i2c buffers are large enough
#ifdef USE_PRODUCT_INFO
    printSerialNumber();
    printModuleVersions();
#endif

    // set a temperature offset in degrees celsius
    // Note: supported by SEN54 and SEN55 sensors
    // By default, the temperature and humidity outputs from the sensor
    // are compensated for the modules self-heating. If the module is
    // designed into a device, the temperature compensation might need
    // to be adapted to incorporate the change in thermal coupling and
    // self-heating of other device components.
    //
    // A guide to achieve optimal performance, including references
    // to mechanical design-in examples can be found in the app note
    // “SEN5x – Temperature Compensation Instruction” at www.sensirion.com.
    // Please refer to those application notes for further information
    // on the advanced compensation settings used
    // in `setTemperatureOffsetParameters`, `setWarmStartParameter` and
    // `setRhtAccelerationMode`.
    //
    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    float tempOffset = 0.0;
    error = sen5x.setTemperatureOffsetSimple(tempOffset);
    if (error) {
        Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Temperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }

    // Start Measurement
    error = sen5x.startMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }


    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    AsyncWiFiManager wm(&server,&dns);

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    //res = wm.autoConnect("SensirionAP","sens"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }

    mqttClient.setBufferSize(512);

    prefs.begin("Sensirion Sensor");
    stateTopic = prefs.getString("mqttStateTopic", "environment/sensirion");
    mqttServerIp = prefs.getString("mqttServerIp", "192.168.1.10");
    mqttServerPort = prefs.getInt("mqttServerPort", 1883);
    mqttEnabled = prefs.getBool("mqttEnabled", true);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", genHtml(stateTopic, mqttServerIp.c_str(), String(mqttServerPort), mqttEnabled));
    });
    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String topic = stateTopic;
        mqttEnabled = false;
        if (request->hasParam(TOPIC_MESSAGE)) {
            topic = request->getParam(TOPIC_MESSAGE)->value();
            stateTopic = topic;
            prefs.putString("mqttStateTopic", topic);
        } 
        if (request->hasParam(SERVER_IP_MESSAGE)) {
            mqttServerIp = request->getParam(SERVER_IP_MESSAGE)->value();
            prefs.putString("mqttServerIp", mqttServerIp);
        }
        if (request->hasParam(SERVER_PORT_MESSAGE)) {
            mqttServerPort = atoi(request->getParam(SERVER_PORT_MESSAGE)->value().c_str());
            prefs.putInt("mqttServerPort", mqttServerPort);
        }
        if (request->hasParam(MQTT_ENABLED_MESSAGE)) {
            mqttEnabled = request->getParam(MQTT_ENABLED_MESSAGE)->value() == "Yes";
        }
        prefs.putBool("mqttEnabled", mqttEnabled); // Always write this to catch the Enable checkbox not checked also.
        
        request->send(200, "text/html", genHtml(topic, mqttServerIp, String(mqttServerPort), mqttEnabled));
    });
    
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        String response;
        serializeJson(sensordata, response);
        request->send(200, "application/json", response);
    });

    server.onNotFound(notFound);
    server.begin();
}


void loop() {
    uint16_t error;
    char errorMessage[256];

    if (mqttEnabled) {
        mqttClient.loop();
    }

    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= 10000) {
        //restart this TIMER
        previousMillis = currentMillis;

        if (mqttEnabled) {
            reconnectMQTT();
        }

        SensirionMeasurement data;

        error = sen5x.readMeasuredValues(
            data.massConcentrationPm1p0, data.massConcentrationPm2p5, data.massConcentrationPm4p0,
            data.massConcentrationPm10p0, data.ambientHumidity, data.ambientTemperature, data.vocIndex,
            data.noxIndex);

        if (error) {
            Serial.print("Error trying to execute readMeasuredValues(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
        } else {
            if (mqttEnabled) {
                sendMQTT(data);
            }
            Serial.print("MassConcentrationPm1p0:");
            Serial.print(data.massConcentrationPm1p0);
            Serial.print("\t");
            Serial.print("MassConcentrationPm2p5:");
            Serial.print(data.massConcentrationPm2p5);
            Serial.print("\t");
            Serial.print("MassConcentrationPm4p0:");
            Serial.print(data.massConcentrationPm4p0);
            Serial.print("\t");
            Serial.print("MassConcentrationPm10p0:");
            Serial.print(data.massConcentrationPm10p0);
            Serial.print("\t");
            Serial.print("AmbientHumidity:");
            if (isnan(data.ambientHumidity)) {
                Serial.print("n/a");
            } else {
                Serial.print(data.ambientHumidity);
            }
            Serial.print("\t");
            Serial.print("AmbientTemperature:");
            if (isnan(data.ambientTemperature)) {
                Serial.print("n/a");
            } else {
                Serial.print(data.ambientTemperature);
            }
            Serial.print("\t");
            Serial.print("VocIndex:");
            if (isnan(data.vocIndex)) {
                Serial.print("n/a");
            } else {
                Serial.print(data.vocIndex);
            }
            Serial.print("\t");
            Serial.print("NoxIndex:");
            if (isnan(data.noxIndex)) {
                Serial.println("n/a");
            } else {
                Serial.println(data.noxIndex);
            }
        }
    }
}
