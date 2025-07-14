#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Menambahkan library ArduinoJson

// Sensor dan LCD
MAX30105 particleSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Pastikan alamat I2C benar

// LED dan Buzzer
const int ledPin = D5; // Pin LED
const int buzzerPin = D6; // Pin Buzzer

// Heart rate data
const byte RATE_SIZE = 4; // Untuk averaging
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;

unsigned long lastSerialPrint = 0; // Untuk pembaruan serial
const int serialPrintInterval = 1500; // Interval 1 detik

// Sensor Piezo
int port_piezo = D2; 
const int numReadings = 10;  // Jumlah pembacaan untuk rata-rata
int readings[numReadings];   // Array untuk menyimpan pembacaan
int readIndex = 0;           // Indeks pembacaan saat ini
int total = 0;               // Jumlah total pembacaan
int average = 0;             // Nilai rata-rata

// MQTT Broker settings
const char* mqtt_server = "192.x.x.x"; // Ganti dengan IP broker MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// WiFi settings
const char* ssid = "";
const char* password = "";

// Setup MQTT
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void reconnect() {
  // Loop sampai terhubung ke broker MQTT
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Setup LED dan Buzzer
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Heart Rate:");

  // Inisialisasi sensor MAX30105
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  // Setup sensor Piezo
  pinMode(port_piezo, INPUT);

  // Inisialisasi pembacaan array dengan 0 untuk piezo
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  // Setup WiFi dan MQTT
  setup_wifi();
  client.setServer(mqtt_server, 1884);
}

void loop() {
  // Connect to MQTT broker if disconnected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Pembacaan sensor MAX30105
  long irValue = particleSensor.getIR();

  if (irValue > 50000) { // Proses hanya jika sinyal IR di atas threshold
    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;

        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
  }

  // Pembacaan sensor Piezo
  total = total - readings[readIndex];  // Kurangi total dengan nilai yang akan diganti
  readings[readIndex] = analogRead(port_piezo);  // Membaca nilai dari sensor piezo
  total = total + readings[readIndex];  // Tambahkan nilai pembacaan ke total
  average = total / numReadings;  // Hitung rata-rata

  readIndex = readIndex + 1;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  // Pembaruan serial dan LCD
  if (millis() - lastSerialPrint >= serialPrintInterval) {
    lastSerialPrint = millis();

// Pembaruan LED dan Buzzer
  if (beatAvg < 60 || beatAvg > 100 || average > 200 ) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }

    // Menampilkan nilai sensor di serial monitor
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);
    Serial.println("Piezo : " + (String)average);

    // Update LCD
    // Tambahkan ini di awal pembaruan tampilan LCD
lcd.clear(); // Membersihkan layar sebelum memperbarui tampilan

// Pastikan setiap pembaruan diatur dengan benar
lcd.setCursor(0, 0);
lcd.print("Piezo : ");
lcd.print(average);
lcd.print("     "); // Tambahkan spasi untuk menghapus nilai sebelumnya

lcd.setCursor(0, 1);
lcd.print("BPM: ");
if (irValue < 50000) {
  lcd.print("No finger  "); // Tambahkan spasi untuk menghapus nilai sebelumnya
} else {
  lcd.print(beatAvg);
  lcd.print("     "); // Tambahkan spasi untuk menghapus nilai sebelumnya
}


    // Kirim data ke MQTT dengan ArduinoJson
    unsigned long timestamp = millis();
    
    // Membuat objek JSON untuk BPM
    StaticJsonDocument<200> bpmDoc;
    bpmDoc["id"] = "bpm_001";
    bpmDoc["timestamp"] = timestamp;
    bpmDoc["tag"] = "heart_rate";
    bpmDoc["value"] = beatAvg;
    
    // Kirim data BPM ke topik khusus BPM
    char bpmPayload[256];
    serializeJson(bpmDoc, bpmPayload);
    client.publish("sensor/bpm", bpmPayload);

    // Membuat objek JSON untuk Piezo
    StaticJsonDocument<200> piezoDoc;
    piezoDoc["id"] = "piezo_001";
    piezoDoc["timestamp"] = timestamp;
    piezoDoc["tag"] = "vibration";
    piezoDoc["value"] = average;
    
    // Kirim data Piezo ke topik khusus Piezo
    char piezoPayload[256];
    serializeJson(piezoDoc, piezoPayload);
    client.publish("sensor/piezo", piezoPayload);
  }
}
