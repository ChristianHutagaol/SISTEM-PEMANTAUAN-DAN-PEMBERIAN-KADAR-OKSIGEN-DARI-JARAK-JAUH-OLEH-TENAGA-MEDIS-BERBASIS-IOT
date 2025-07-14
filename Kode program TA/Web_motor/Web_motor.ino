#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Definisi pin motor
#define RPWM D6
#define LPWM D5
#define PWM D7

// Waktu delay untuk satu putaran penuh motor (misal 5 detik untuk 1 putaran)
unsigned long waktuPutaran = 15;  // 5000 milidetik = 5 detik

// Data WiFi
const char* ssid = "";      // Ganti dengan nama jaringan WiFi
const char* password = "";  // Ganti dengan password WiFis

ESP8266WebServer server(80);  // Membuat Web Server pada port 80

int jumlahPutaran = 0; // Menyimpan jumlah putaran yang diinginkan
String arahPutaran = ""; // Menyimpan arah putaran yang dipilih

// Fungsi untuk menghubungkan ESP32 ke WiFi
void setup_wifi() {
  Serial.print("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Terhubung ke WiFi");
  // Menampilkan alamat IP setelah terhubung
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());
}

// Fungsi untuk motor berputar sesuai arah
void putar_motor() {
  for (int i = 0; i < jumlahPutaran; i++) {
    if (arahPutaran == "kanan") {
      motor_ccw(); // Berputar ke kanan
    } else if (arahPutaran == "kiri") {
      motor_cw(); // Berputar ke kiri
    }
    delay(1000); // Berikan sedikit delay antar putaran
  }
  arahPutaran = ""; // Reset arah setelah selesai
  jumlahPutaran = 0; // Reset jumlah putaran setelah selesai
}

// Fungsi untuk motor berputar ke kiri
void motor_cw() {
  digitalWrite(LPWM, LOW);
  digitalWrite(RPWM, HIGH);
  analogWrite(PWM, 400);  // Kecepatan motor (PWM)
  Serial.println("Motor berputar ke kiri.");
  delay(waktuPutaran); // Motor berputar selama 5 detik (1 putaran)
  motor_stop(); // Setelah 5 detik, motor berhenti
}

// Fungsi untuk motor berputar ke kanan
void motor_ccw() {
  digitalWrite(LPWM, HIGH);
  digitalWrite(RPWM, LOW);
  analogWrite(PWM, 400);  // Kecepatan motor (PWM)
  Serial.println("Motor berputar ke kanan.");
  delay(waktuPutaran); // Motor berputar selama 5 detik (1 putaran)
  motor_stop(); // Setelah 5 detik, motor berhenti
}

// Fungsi untuk menghentikan motor
void motor_stop() {
  digitalWrite(LPWM, LOW);
  digitalWrite(RPWM, LOW);
  analogWrite(PWM, 0);  // Stop motor
  Serial.println("Motor berhenti.");
}

// Fungsi untuk mengatur dan menampilkan halaman web
void handleRoot() {
  String html = "<html><head>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f4f4f9; }";
  html += ".container { text-align: center; padding: 20px; border-radius: 8px; background-color: #ffffff; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }";
  html += "h1 { color: #333; }";
  html += "label { font-size: 16px; color: #555; }";
  html += "input[type='number'], input[type='radio'] { margin: 8px 0; padding: 8px; font-size: 16px; }";
  html += "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  html += "input[type='submit']:hover { background-color: #45a049; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Kontrol Motor (Pemberian Oksigen)</h1>";
  html += "<form action='/motor' method='get'>";
  html += "<label for='arah'>Pilih Arah Putaran:</label><br>";
  html += "<input type='radio' name='arah' value='kanan'> Kanan (menutup) <br>";
  html += "<input type='radio' name='arah' value='kiri'> Kiri (membuka) <br><br>";
  html += "<label for='jumlah'>Jumlah Putaran (1-15):</label><br>";
  html += "<input type='number' name='jumlah' min='1' max='15' required><br><br>";
  html += "<input type='submit' value='Kirim'>";
  html += "</form>";
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


// Fungsi untuk menangani form pengontrolan motor
void handleMotorControl() {
  if (server.hasArg("arah") && server.hasArg("jumlah")) {
    arahPutaran = server.arg("arah");
    jumlahPutaran = server.arg("jumlah").toInt();
    Serial.print("Arah: ");
    Serial.println(arahPutaran);
    Serial.print("Jumlah Putaran: ");
    Serial.println(jumlahPutaran);
    putar_motor();  // Menjalankan motor berdasarkan kontrol
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(9600);
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(PWM, OUTPUT);

  // Menghubungkan ke WiFi
  setup_wifi();

  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/motor", HTTP_GET, handleMotorControl);

  server.begin();  // Memulai Web Server
  Serial.println("Server web dimulai");
}

void loop() {
  server.handleClient();  // Menangani permintaan HTTP dari web server
}
