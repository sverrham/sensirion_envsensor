
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

#include <ArduinoMqttClient.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>

// The used commands use up to 48 bytes. On some Arduino's the default buffer
// space is not large enough
#define MAXBUF_REQUIREMENT 48

#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif

SensirionI2CSen5x sen5x;

String hw_version;
String sw_version;

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

unsigned char serialNumber[32];

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


WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


void publishMQTT(JsonDocument doc, String topic_dev, bool retain) {
    // Serialize the JSON document to a char buffer
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    const char* payload = jsonBuffer;
    mqttClient.beginMessage(topic_dev, retain=retain);
    mqttClient.print(payload);
    mqttClient.endMessage();
    Serial.println("Published message: "+ topic_dev + String(payload));
}

// My numeric sensor ID, you can change this to whatever suits your needs
int sensorNumber = 1;
// This is the topic this program will send the state of this device to.
String stateTopic = "environment/sensirion/garage";

JsonDocument getDeviceInfo(){
    JsonDocument devDoc;
    devDoc["ids"] = serialNumber; //identifiers
    devDoc["mf"] = "Sham(Sensirion)"; //manufacturer
    devDoc["mdl"] = "SEN55";  //model
    devDoc["name"] = "Sensirion SEN55";
    devDoc["hw"] = hw_version; //hw_version
    devDoc["sw"] = sw_version; //sw_version

    return devDoc;
}

void sendMQTTPm1p0DiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/pm1p0/config";

    doc["name"] = "Env " + String(sensorNumber) + " Pm1p0";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "μg/mᵌ";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.pm1p0|default(0) }}";
    doc["uniq_id"] = String(ESP.getChipId()) + "_pm1p0";
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}

void sendMQTTPm2p5DiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/pm2p5/config";

    doc["name"] = "Env " + String(sensorNumber) + " Pm2p5";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "μg/mᵌ";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.pm2p5|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_pm2p5";
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}

void sendMQTTPm4p0DiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/pm4p0/config";

    doc["name"] = "Env " + String(sensorNumber) + " Pm4p0";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "μg/mᵌ";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.pm4p0|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_pm4p0";;
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}

void sendMQTTPm10p0DiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/pm10p0/config";

    doc["name"] = "Env " + String(sensorNumber) + " Pm10p0";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "μg/mᵌ";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.pm10p0|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_pm10p0";;
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}

void sendMQTTTemperatureDiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/temperature/config";

    doc["name"] = "Env " + String(sensorNumber) + " Temperature";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "⁰C";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.temperature|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_temp";;
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}

void sendMQTTHumidityDiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/humidity/config";

    doc["name"] = "Env " + String(sensorNumber) + " Humidity";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "%RH";
    doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.humidity|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_humid";
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}


void sendMQTTVocIndexDiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/vocindex/config";

    doc["name"] = "Env " + String(sensorNumber) + " VocIndex";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "";
    // doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.vocIndex|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_voci";
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
}


void sendMQTTNoxIndexDiscoveryMsg() {
    JsonDocument doc;
    String discoveryTopic = "homeassistant/sensor/env_sensor_" + String(sensorNumber) + "/noxindex/config";

    doc["name"] = "Env " + String(sensorNumber) + " NoxIndex";
    doc["stat_t"]   = stateTopic;
    doc["unit_of_meas"]   = "";
    // doc["sug_dsp_prc"] = 2; //         'suggested_display_precision',
    doc["frc_upd"] = true;
    doc["val_tpl"] = "{{ value_json.noxIndex|default(0) }}";

    doc["uniq_id"] = String(ESP.getChipId()) + "_noxi";;
    doc["device"] = getDeviceInfo();

    publishMQTT(doc, discoveryTopic, true);
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

    publishMQTT(doc, stateTopic, false);
}


void reconnectMQTT() {

    const char broker[] = "adg";
    int        port     = 1883;

    if (!mqttClient.connected()) {
        while (!mqttClient.connect(broker, port)) {
            delay (10000);
            Serial.println("MQTT Not Connected");
        }
        // Send discovery messages
        sendMQTTPm1p0DiscoveryMsg();
        sendMQTTPm2p5DiscoveryMsg();
        sendMQTTPm4p0DiscoveryMsg();
        sendMQTTPm10p0DiscoveryMsg();
        sendMQTTTemperatureDiscoveryMsg();
        sendMQTTHumidityDiscoveryMsg();
        sendMQTTVocIndexDiscoveryMsg();
        sendMQTTNoxIndexDiscoveryMsg();
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
    WiFiManager wm;

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
}


void loop() {
    uint16_t error;
    char errorMessage[256];

    delay(10000);

    reconnectMQTT();

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
        sendMQTT(data);
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
