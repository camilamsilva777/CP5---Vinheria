#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
// ---------- DEFINIÇÕES DOS SENSORES ----------
#define DHTPIN   15        // DHT22 no GPIO15
#define DHTTYPE  DHT22
#define LDR_PIN  34        // LDR no ADC GPIO34
#define TRIG_PIN 5         // HC-SR04 TRIG no GPIO5
#define ECHO_PIN 18        // HC-SR04 ECHO no GPIO18

DHT dht(DHTPIN, DHTTYPE);

// ---------- REDE / THINGSPEAK ----------
const char* ssid     = "Wokwi-GUEST";
const char* password = "";
const char* server   = "http://api.thingspeak.com";
const char* apiKey   = "KGC2JE6OQJFPZ6SR"; // sua Write API Key

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Wi-Fi
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" conectado!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // ---- Leituras dos sensores ----
    float h = dht.readHumidity();
    float t = dht.readTemperature(); // Celsius
    if (isnan(h) || isnan(t)) {
      Serial.println("Falha ao ler o DHT22. Tentando novamente...");
      delay(2000);
      return;
    }

    int ldrRaw = analogRead(LDR_PIN);            // 0..4095
    float luminosidade = (ldrRaw / 4095.0) * 100.0; // % aproximada

    // HC-SR04 (distância em cm)
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
    float distance = (duration * 0.0343) / 2.0;     // cm

    // ---- Logs no Serial ----
    Serial.print("Temperatura: "); Serial.print(t, 1); Serial.println(" °C");
    Serial.print("Umidade: ");     Serial.print(h, 0); Serial.println(" %");
    Serial.print("Luminosidade: ");Serial.print(luminosidade, 0); Serial.println(" %");
    Serial.print("Distancia: ");   Serial.print(distance, 1); Serial.println(" cm");

    // ---- Envio ao ThingSpeak ----
    // field1 = temperatura, field2 = umidade, field3 = luminosidade, field4 = distancia
    String url = String(server) + "/update?api_key=" + apiKey +
                 "&field1=" + String(t, 1) +
                 "&field2=" + String(h, 0) +
                 "&field3=" + String(luminosidade, 0) +
                 "&field4=" + String(distance, 1);

    HTTPClient http;
    http.begin(url);
    int code = http.GET(); // ThingSpeak suporta GET simples
    Serial.print("HTTP status: "); Serial.println(code);
    http.end();

  } else {
    Serial.println("WiFi desconectado. Tentando reconectar...");
    WiFi.reconnect();
  }

  // IMPORTANTE: ThingSpeak free precisa de ~15s entre atualizações
  delay(15000);
}
