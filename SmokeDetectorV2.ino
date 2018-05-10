#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char * MQTT_SERVER = "192.168.0.110";
const char * MQTT_USER = "admin";
const char * MQTT_PASS = "root";
const char * MQTT_TOPIC = "/admin/smoke/";

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

int mqttTimeout = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("ESP Booted");
  String ssid = readEEPROM(0, 40);
  String password = readEEPROM(41, 80);
  setup_wifi(ssid, password);
  setup_ssdp();
  setup_webserver();
  setup_accesspoint();
  setup_mqtt();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
  if (!client.connected() && mqttTimeout++ > 300000) {
    mqttTimeout = 0;
    connect_mqtt();
  } else {
  client.loop();
  }
}
/*
 * CUSTOM FUNCTIONS
 */
 int setup_wifi(String ssid, String password) {
  Serial.print("Connecting");
  int count = 0;
   WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if(count++ == 10) {
      Serial.println("\nFail connecting to wifi");
      WiFi.disconnect(true);
      return 0;
    }
  }
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());\
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
  Serial.println("\nConnected.");
  writeEEPROM(0, 40, ssid);
  writeEEPROM(41, 80, password);
  return 1;
 }
 void setup_ssdp() {
   SSDP.setName("Ecell Smoke Detector");
   SSDP.setSerialNumber("001788102201");
   SSDP.setModelName("Smoke Detector");
   SSDP.setModelNumber("929000226503");
   SSDP.setModelURL("http://www.bhkdev2.tk");
   SSDP.setManufacturer("bhaktij");
   SSDP.setManufacturerURL("http://www.bhkdev2.tk");
   SSDP.begin();
 }
 void setup_webserver() {
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/connect", handleConnect);
  server.begin();
  Serial.println("Web Server Started");
 }
 void setup_accesspoint() {
  Serial.println("Configuring access point...");
  WiFi.softAP("smokedetectorV2");
 }
 void setup_mqtt() {
  client.setServer(MQTT_SERVER, 1883);
 }
 void connect_mqtt() { 
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("SmokeDetectorClient", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      client.publish(MQTT_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(MQTT_TOPIC);
    } else {
      Serial.println("failed");
    }
 }
 /*
  *  ROM FUNCTIONS
  */
void writeEEPROM(int startAdr, int laenge, String writeString) {
  EEPROM.begin(512); //Max bytes of eeprom to use
  yield();
  Serial.println();
  Serial.print("writing EEPROM: ");
  //write to eeprom 
  for (int i = 0; i < laenge; i++)
    {
      EEPROM.write(startAdr + i, writeString[i]);
      Serial.print(writeString[i]);
    }
  EEPROM.commit();
  EEPROM.end();           
}

String readEEPROM(int startAdr, int maxLength) {
  EEPROM.begin(512);
  delay(10);
  String dest;
  for (int i = 0; i < maxLength; i++)
    {
      dest += char(EEPROM.read(startAdr + i));
    }
  EEPROM.end();    
  Serial.print("ready reading EEPROM:");
  Serial.println(dest);
  return dest;
}
  
/*
* Web Server Handlers
*/
void handleRoot() {
  server.send(200, "text/plain", "1");
}
void handleScan() {
  Serial.println("Request for scan");
  String data = "{\"networks\":[";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    data += "{\"name\":\"";
    data += WiFi.SSID(i);
    data += "\",\"encryption\":\"";
    data += WiFi.encryptionType(i);
    data += "\"}";
    if(i!=n-1) data += ",";
  }
  data  += "]}";
  Serial.println(data);
  server.send(200, "text/plain", data);
}
void handleConnect() {
  Serial.println("Request for connect");
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));
  String ssid = root["name"];
  String password = root["password"];
  if(setup_wifi(ssid, password) == 1) {
    Serial.println("Connected.");
    server.send(200, "text/plain", "1");
  } else {
    server.send(200, "text/plain", "0");
  }
}
