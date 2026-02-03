// RX (to DY-SV8F TX): GPIO 20 (often labeled RX or U0RXD — default UART0 RX 1K series)
// TX (to DY-SV8F RX): GPIO 21 (often labeled TX or U0TXD — default UART0 TX 1K serie

#include <WiFi.h>                // 
#include <PubSubClient.h>
#include <HardwareSerial.h>      // Built-in, no extra library needed
#include <DYPlayerArduino.h>
#include <ArduinoJson.h>

// Pins - adjust if your labeling differs (check with multimeter or blink test)
// Change this line:

// And pick two free GPIOs, e.g.:
#define PLAYER_RX_PIN 10     // Example: GPIO10 → DY-SV8F TX
#define PLAYER_TX_PIN 7      // Example: GPIO7  → DY-SV8F RX

//#define PLAYER_RX_PIN 20     // GPIO20 → DY-SV8F TX
//#define PLAYER_TX_PIN 21     // GPIO21 → DY-SV8F RX

//HardwareSerial playerSerial(0);  // ← Use UART1 instead of 0
HardwareSerial playerSerial(1);  // UART0 (default console UART, but remappable & works)
DY::Player player(&playerSerial);

// ── WiFi & MQTT ──────────────────────────────────────────────────────────────
const char* ssid       = "Wifi-SSID";              // WiFi SSID
const char* password   = "WiFi Password";   // WiFi Password
const char* mqtt_server = "Brokey IP";     // Broker IP
const char* mqtt_user     = "mqtt username";   // mqtt username
const char* mqtt_password = "mqtt password";       // mqtt password
const int   mqtt_port  = 1883;
const char* mqtt_topic_in = "alert/notifier";     // subscribe here
const char* mqtt_topic_out = "alert/status";     // subscribe here
const char* mqtt_topic_played = "alert/played";     // subscribe here
 
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 5UL * 60 * 1000;  // 5 minutes in milliseconds

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
  char played[5];
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
    itoa( file, played, 10 );
    client.publish(mqtt_topic_played, played);
  }
  //while(!ready_pin())
  //  client.publish(mqtt_topic_out, "connected");  
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

  // Periodic heartbeat
  unsigned long now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    
    // Only publish if we're actually connected
    if (client.connected()) {
      client.publish(mqtt_topic_out, "connected");
      Serial.println("Published heartbeat: connected");
    } else {
      Serial.println("Heartbeat skipped - MQTT not connected");
    }
  }
}
