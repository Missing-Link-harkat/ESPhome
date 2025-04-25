#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ElegantOTA.h>
#include <DHT.h>

// Wi-Fi Configuration
const char* ssid = "";                          // Write your Wi-Fi SSID
const char* password = "";                      // Write your Wi-Fi password

// MQTT Configuration
const char* mqtt_server = "";          //MQTT servers IP address (usually same IP address as Home Assistant server)

WiFiClient espClient;
PubSubClient client(espClient);

// Timing For MQTT Message
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;  // Unused counter but kept for MQTT message incrementing (if needed)

// Web Server For OTA
WebServer server(80);   // HTTP server runs on port 80

// DHT Sensor Configuration
#define DHTPIN 0            // GPIO 0 is used for the DHT11 data line
#define DHTTYPE DHT11       // Define the sensor type: DHT11

DHT dht(DHTPIN, DHTTYPE);   // Create DHT object

// Connect to Wi-Fi
void setup_wifi() {
  delay(10);    // Short delay before starting
  
  WiFi.mode(WIFI_STA);          // Set ESP tp Sation mode
  WiFi.begin(ssid, password);   // Connect to WiFi
  Serial.println("");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // WiFi connected
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());   // Prints assingned IP

}

// MQTT Callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);   // Prints payload as characters
  }
  Serial.println();
}

// MQTT reconnection
void reconnect() {
  // Loop until we're connected to MQTT
  while(!client.connected()) {
      Serial.print("Attempting MQTT connection...");

      // Generate unique client ID
      String clientId = "ESP32Client-";         // You can change ID your own
      clientId += String(random(0xffff), HEX);

      // Attempt to connect
      if (client.connect(clientId.c_str())) {
        Serial.print("Connected!");
      }
      else {
        Serial.print("Failed, rc=");
        Serial.print(client.state());
        delay(5000);  // Wait before retrying
      }
  }
}

void setup(void) {
  Serial.begin(115200);

  // Start WiFi
  setup_wifi();

  // Define a simple web server endpoint
  server.on("/", []() {
    server.send(200, "text/plain", "IT IS ON");   // Respond to "/" with plain text
  });
 
  // Setup MQTT
  client.setServer(mqtt_server, 1883);    // MQTT server and port
  client.setCallback(callback);           // MQTT message callback

  ElegantOTA.begin(&server);             // OTA over HTTP
  server.begin();                        // Start the web server
  Serial.println("HTTP server started");

  dht.begin();                          // Start DHT sensor
} 

void loop(void) {
  server.handleClient();    // Handle incoming web requests
  ElegantOTA.loop();        // Handle OTA updates

  delay(2000);    // Wait 2 secands before reading the sensor

  // Read humidity and temperature sensor values
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  else {

  // Print values to Serial Monitor
  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.print(" °C ");
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F(" %\n"));

  Serial.println("Sending alert via MQTT...");

  // Ensure MQTT is connected
  if (!client.connected()) {
    reconnect();
  }

  // Send data via MQTT every 2 seconds
  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;

    // Format sensor data as JSON string with temp and humidity
    snprintf (msg, MSG_BUFFER_SIZE, "{\"temperature\": %4.1f, \"humidity\": %4.1f}", t, h);

    // Publish to topic
    Serial.print("Publish message: ");
    Serial.println(msg);

    client.publish("DHT11/01", msg);  // You can change this topic
    }
  }
  delay(5000); // Wait before next reading (adjust as needed)
}