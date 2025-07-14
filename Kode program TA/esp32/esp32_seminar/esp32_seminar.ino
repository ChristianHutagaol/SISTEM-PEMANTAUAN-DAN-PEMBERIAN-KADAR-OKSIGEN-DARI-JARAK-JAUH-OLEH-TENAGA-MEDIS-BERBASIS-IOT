#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h> // Library untuk LCD I2C
#include <ArduinoJson.h>

MAX30105 particleSensor;

// Konfigurasi Wi-Fi dan MQTT
#define wifi_ssid ""
#define wifi_password ""
#define mqtt_server "192.x.x.x"

#define bpm_topic "sensor/SpO2"

WiFiClient espClient;
PubSubClient client(espClient);

#define MAX_BRIGHTNESS 255

// Pin untuk LED dan buzzer
#define LED_PIN 14    // Pin LED (GPIO 14)
#define BUZZER_PIN 2  // Pin Buzzer (GPIO 2)

uint32_t irBuffer[50]; // infrared LED sensor data
uint32_t redBuffer[50]; // red LED sensor data

int32_t bufferLength; // data length
int32_t spo2; // SPO2 value
int8_t validSPO2; // indikator untuk menunjukkan apakah perhitungan SPO2 valid
int32_t heartRate; // nilai detak jantung
int8_t validHeartRate; // indikator untuk menunjukkan apakah perhitungan detak jantung valid

// Konfigurasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C LCD (0x27 untuk banyak modul)

void setup() {
  Serial.begin(115200); // komunikasi serial pada 115200 bps

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  Serial.println("Initializing...");

  // Inisialisasi LCD
  lcd.init(); 
  lcd.backlight(); 
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(2000);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Inisialisasi sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30105 tidak ditemukan. Periksa wiring/power."));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    while (1);
  }

  Serial.println(F("Pasang sensor ke jari dengan karet."));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger");
  lcd.setCursor(0, 1);
  lcd.print("on sensor.");
  
  byte ledBrightness = 60; // Kecerahan LED: 0=Off to 255=50mA
  byte sampleAverage = 4; // Pengambilan sampel rata-rata: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; // Mode LED: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 50; // Frekuensi sampel: 50 sps
  int pulseWidth = 215; // Lebar pulsa
  int adcRange = 4096; // Rentang ADC

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  bufferLength = 50;

  // Hitung detak jantung dan SpO2
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)
      particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  // Monitoring SpO2 dan HR di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  if (spo2 == -999) {
    lcd.print("SpO2: Calculating");
  } else {
    lcd.print("SpO2: ");
    lcd.print(spo2);
    lcd.print("%");
  }

  lcd.setCursor(0, 1);
  lcd.print("HR: ");
  lcd.print(validHeartRate);

  Serial.print("HR: ");
  Serial.print(heartRate);

  // Jika SpO2 < 95, nyalakan LED dan buzzer
  if (spo2 < 95 && spo2 != -999 || spo2 > 100) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Kirim data ke MQTT dalam format JSON
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["id"] = "spo2_001";  // Ganti dengan ID unik Anda
  jsonDoc["timestamp"] = millis();
  jsonDoc["tag"] = "Kadar Oksigen"; // Timestamp menggunakan millis()
  jsonDoc["value"] = spo2;

  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);

  client.publish(bpm_topic, jsonBuffer, true);

  delay(500);
}
