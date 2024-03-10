

#include <Arduino.h>


#include <PubSubClient.h>
#include <ESPAsyncWiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <Preferences.h>

#include "ha_discovery.h"
#include "sensirion.h"

Preferences prefs;

AsyncWebServer server(80);
DNSServer dns;

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
        ha_discovery.setDeviceInfo(getSen5xSerialNumber(), getSen5xHwVersion(), getSen5xSwVersion());
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

    sen5xSetup();

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
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    SensirionMeasurement data;

    if (mqttEnabled) {
        mqttClient.loop();
    }

    if (currentMillis - previousMillis >= 10000) {
        //restart this TIMER
        previousMillis = currentMillis;
       
        data = readSen5xData();

        if (mqttEnabled) {
            reconnectMQTT();
            sendMQTT(data);
        }
    }
}
