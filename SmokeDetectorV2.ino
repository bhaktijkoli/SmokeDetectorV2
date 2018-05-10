#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char * MQTT_SERVER = "bhkdev2.tk";
const int MQTT_PORT = 3011;
const char * MQTT_USER = "admin";
const char * MQTT_PASS = "root";
const char * MQTT_TOPIC = "/admin/smoke/";
const int INPUT_PIN = D2;
const int MQTT_INTERVAL = 300000;
const int SLEEPINTERVAL = 50000;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

int mqttInterval = MQTT_INTERVAL;
int sleepInterval = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(INPUT_PIN, INPUT);
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
  if (!client.connected() && mqttInterval++ > MQTT_INTERVAL) {
    mqttInterval = 0;
    connect_mqtt();
  }
  if(client.connected()) {
    client.loop();
    if(digitalRead(INPUT_PIN) == HIGH) {
      client.publish(MQTT_TOPIC, "alert");
      delay(10000);
    }
  }
  if(WiFi.status() == WL_CONNECTED && sleepInterval++ > SLEEPINTERVAL) {
    delay(500);
    Serial.println("Going to sleep mode");
    ESP.deepSleep(0);
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
  sleepInterval=0;
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
  client.setServer(MQTT_SERVER, MQTT_PORT);
}
void connect_mqtt() { 
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect("SmokeDetectorClient", MQTT_USER, MQTT_PASS)) {
    Serial.println("connected");
    client.subscribe(MQTT_TOPIC);
  } else {
    Serial.println("failed");
  }
  sleepInterval=0;
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
  sleepInterval=0;
  server.send(200, "text/plain", "1");
}
void handleScan() {
  sleepInterval=0;
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
  sleepInterval=0;
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

