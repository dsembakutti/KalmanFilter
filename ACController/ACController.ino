// Include the required libraries
#include "DHT.h"
#include "WiFi.h"
#include "ThingSpeak.h"
#include <BlynkSimpleEsp32.h>

// --- Wi-Fi and ThingSpeak Settings ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
unsigned long myChannelNumber = 123456; // Your ThingSpeak Channel ID
const char * myWriteAPIKey = "YOUR_WRITE_API_KEY"; // Your ThingSpeak Write API Key

// --- Blynk Settings ---
char auth[] = "YOUR_BLYNK_AUTH_TOKEN"; // Your Blynk Auth Token

// Define the sensor and relay pins
#define DHTPIN 4     // The ESP32 pin connected to the DHT22 sensor
#define RELAY_PIN 5  // The ESP32 pin connected to the relay module

// Define the sensor type
#define DHTTYPE DHT22   // We are using the DHT22 sensor

// Initialize the DHT sensor and WiFi client
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

void setup() {
  // Start the serial communication
  Serial.begin(115200);
  Serial.println("AC Controller");

  // Initialize the DHT sensor
  dht.begin();

  // Set the relay pin as an output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Turn the AC off by default

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize ThingSpeak
  ThingSpeak.begin(client);

  // Initialize Blynk
  Blynk.begin(auth, ssid, password);
}

// --- Control Settings ---
float temp_threshold_high = 25.0; // Turn AC ON above this temperature
float temp_threshold_low = 23.0;  // Turn AC OFF below this temperature
float hum_threshold_high = 60.0;  // Turn AC ON above this humidity

// --- State Variables ---
bool ac_on = false;
bool manual_override = false;
unsigned long ac_on_time = 0;
unsigned long ac_off_time = 0;
unsigned long last_switch_time = 0;
unsigned long last_thingspeak_update = 0;


void loop() {
  Blynk.run();

  // Wait a few seconds between measurements
  delay(2000);

  // Read the humidity and temperature
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(); // Reads temperature in Celsius

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // --- Automatic Control Logic ---
  if (!manual_override) {
    if (temperature > temp_threshold_high || humidity > hum_threshold_high) {
      if (!ac_on) {
        set_ac_state(true);
      }
    } else if (temperature < temp_threshold_low) {
      if (ac_on) {
        set_ac_state(false);
      }
    }
  }

  // --- Print Status ---
  print_status(humidity, temperature);

  // --- Handle Manual Control (via Serial) ---
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == '1') {
      manual_override = true;
      set_ac_state(true);
      Serial.println("Manual Override: AC ON");
    } else if (command == '0') {
      manual_override = true;
      set_ac_state(false);
      Serial.println("Manual Override: AC OFF");
    } else if (command == 'a') {
      manual_override = false;
      Serial.println("Automatic control enabled");
    }
  }

  // --- Update ThingSpeak ---
  unsigned long current_time = millis();
  if (current_time - last_thingspeak_update > 20000) { // Update every 20 seconds
    ThingSpeak.setField(1, temperature);
    ThingSpeak.setField(2, humidity);
    ThingSpeak.setField(3, ac_on ? 1 : 0);
    ThingSpeak.setField(4, ac_on_time / 1000);
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    last_thingspeak_update = current_time;
    Serial.println("Data sent to ThingSpeak");
  }

  // --- Update Blynk ---
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, ac_on ? 255 : 0);
  Blynk.virtualWrite(V3, ac_on_time / 1000);
}

// --- Blynk Functions ---

// Manual ON Button
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    manual_override = true;
    set_ac_state(true);
  }
}

// Manual OFF Button
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    manual_override = true;
    set_ac_state(false);
  }
}

// Automatic Mode Button
BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    manual_override = false;
  }
}

void set_ac_state(bool new_state) {
  if (ac_on != new_state) {
    ac_on = new_state;
    digitalWrite(RELAY_PIN, ac_on ? HIGH : LOW);
    unsigned long current_time = millis();

    if (ac_on) {
      ac_off_time += current_time - last_switch_time;
      Serial.println("AC turned ON");
    } else {
      ac_on_time += current_time - last_switch_time;
      Serial.println("AC turned OFF");
    }
    last_switch_time = current_time;
  }
}

void print_status(float humidity, float temperature) {
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" *C\t");
  Serial.print("AC Status: ");
  Serial.print(ac_on ? "ON" : "OFF");
  Serial.print("\tON time: ");
  Serial.print(ac_on_time / 1000);
  Serial.print("s\tOFF time: ");
  Serial.print(ac_off_time / 1000);
  Serial.println("s");
}
