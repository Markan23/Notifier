// RX (to DY-SV8F TX): GPIO 20 (often labeled RX or U0RXD — default UART0 RX 1K series)
// TX (to DY-SV8F RX): GPIO 21 (often labeled TX or U0TXD — default UART0 TX 1K series)

#include <WiFi.h>                // Instead of ESP8266WiFi.h
#include <PubSubClient.h>
#include <HardwareSerial.h>      // Built-in, no extra library needed
#include <DYPlayerArduino.h>
#include <ArduinoJson.h>

// Pins - adjust if your labeling differs (check with multimeter or blink test)
#define PLAYER_RX_PIN 20     // GPIO20 → DY-SV8F TX
#define PLAYER_TX_PIN 21     // GPIO21 → DY-SV8F RX

HardwareSerial playerSerial(0);  // UART0 (default console UART, but remappable & works)
DY::Player player(&playerSerial);

// ── WiFi & MQTT ──────────────────────────────────────────────────────────────
const char* ssid       = "my_ssid";              // WiFi SSID
const char* password   = "my_wifi_password";   // WiFi Password
const char* mqtt_server = "MQTT broker ip";     // Broker IP
const char* mqtt_user     = "MQTT Username";   // mqtt username
const char* mqtt_password = "MQTT Password";       // mqtt password
const int   mqtt_port  = 1883;
const char* mqtt_topic_in = "alert/notifier";     // subscribe here
const char* mqtt_topic_out = "alert/status";     // subscribe here

// ── MQTT ─────────────────────────────────────────────────────────────────────
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message on [");
  Serial.print(topic);
  Serial.print("] → ");

  // Print raw payload for debugging
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Parse JSON
  StaticJsonDocument<256> doc;   // Small size — enough for our needs
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  // ── Extract values ────────────────────────────────────────────────────────
  int file   = doc["file"]   | -1;     // -1 = not present
  int volume = doc["volume"] | -1;
  int eq     = doc["eq"]     | -1;     // Optional EQ preset (0-10)

  // Apply volume if provided (0-30)
  if (volume >= 0 && volume <= 30) {
    player.setVolume(volume);
    Serial.print("Volume set to: "); Serial.println(volume);
  }

  // Apply EQ if provided (optional)
  if (eq >= 0 && eq <= 10) {
    player.setEq(static_cast<DY::eq_t>(eq));
    Serial.print("EQ set to: "); Serial.println(eq);
  }

  // Play file if provided
  if (file > 0) {   // Files usually start from 1 (00001.mp3)
    player.setPlayingDevice(DY::Device::Flash);   // or SD if you use card
    player.playSpecified(file);                   // Plays 0000X.mp3 where X = file
    Serial.print("Playing file: "); Serial.println(file);
    client.publish(mqtt_topic_out, "alert");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID (good practice to avoid conflicts)
    String clientId = "ESP32C3Notifier-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect WITH username & password
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      
      // Once connected, subscribe to our topic
      client.subscribe(mqtt_topic_in);
      
      // Optional: Publish a "I'm alive" message on connect
      client.publish(mqtt_topic_out, "connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" → try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);  // USB debug console (works over USB-C)

  // Start UART for DY-SV8F (baud 9600, 8N1, assign pins)
  playerSerial.begin(9600, SERIAL_8N1, PLAYER_RX_PIN, PLAYER_TX_PIN);

  player.begin();

  player.setVolume(20);
  player.setCycleMode(DY::PlayMode::OneOff);
  player.setPlayingDevice(DY::Device::Flash);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// Rest of code (loop, reconnect, callback) unchanged!

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
