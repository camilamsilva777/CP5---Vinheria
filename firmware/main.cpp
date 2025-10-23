#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* default_SSID = "Wokwi-GUEST";
const char* default_PASSWORD = "";
const char* default_BROKER_MQTT = "172.24.240.1";
const int default_BROKER_PORT = 1883;
const char* default_TOPICO_SUBSCRIBE = "/TEF/lamp011/cmd";
const char* default_TOPICO_PUBLISH_1 = "/TEF/lamp011/attrs";
const char* default_TOPICO_PUBLISH_2 = "/TEF/lamp011/attrs/l"; 
const char* default_TOPICO_PUBLISH_3 = "/TEF/lamp011/attrs/t"; 
const char* default_TOPICO_PUBLISH_4 = "/TEF/lamp011/attrs/h"; 
const char* default_TOPICO_PUBLISH_5 = "/TEF/lamp011/attrs/u"; 
const char* default_ID_MQTT = "fiware_001";
const int default_D4 = 2;

WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

const char* topicPrefix = "lamp011";

#define DHTPIN 15
#define DHTTYPE DHT22
#define LDR_PIN 34
#define TRIG_PIN 5
#define ECHO_PIN 18

DHT dht(DHTPIN, DHTTYPE);

void initSerial() {
  Serial.begin(115200);
}

void initWiFi() {
  delay(10);
  Serial.println("------ Conectando ao Wi-Fi ------");
  Serial.print("Rede: ");
  Serial.println(default_SSID);
  WiFi.begin(default_SSID, default_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void initMQTT() {
  MQTT.setServer(default_BROKER_MQTT, default_BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

void InitOutput() {
  pinMode(default_D4, OUTPUT);
  digitalWrite(default_D4, LOW);
  for (int i = 0; i < 5; i++) {
    digitalWrite(default_D4, !digitalRead(default_D4));
    delay(200);
  }
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Conectando ao broker MQTT: ");
    Serial.println(default_BROKER_MQTT);
    if (MQTT.connect(default_ID_MQTT)) {
      Serial.println("Conectado ao broker MQTT!");
      MQTT.subscribe(default_TOPICO_SUBSCRIBE);
    } else {
      Serial.println("Falha na conexão MQTT. Tentando novamente em 2s...");
      delay(2000);
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("- Mensagem recebida: ");
  Serial.println(msg);

  if (msg.indexOf("@on|") >= 0) {
    digitalWrite(default_D4, HIGH);
    EstadoSaida = '1';
  } else if (msg.indexOf("@off|") >= 0) {
    digitalWrite(default_D4, LOW);
    EstadoSaida = '0';
  }
}

void readSensors() {
  // --- DHT22 ---
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler DHT22!");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.print(" °C | Umidade: ");
    Serial.print(umidade);
    Serial.println(" %");

    String t_str = String(temperatura);
    String h_str = String(umidade);
    MQTT.publish(default_TOPICO_PUBLISH_3, t_str.c_str());
    MQTT.publish(default_TOPICO_PUBLISH_4, h_str.c_str());
  }

  int ldrValue = analogRead(LDR_PIN);
  int luminosity = map(ldrValue, 0, 4095, 0, 100);
  Serial.print("Luminosidade: ");
  Serial.print(luminosity);
  Serial.println(" %");
  MQTT.publish(default_TOPICO_PUBLISH_2, String(luminosity).c_str());

  long duration;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;

  Serial.print("Distância: ");
  Serial.print(distance);
  Serial.println(" cm");
  MQTT.publish(default_TOPICO_PUBLISH_5, String(distance).c_str());
}

void VerificaConexoesWiFIEMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();
  }
  if (!MQTT.connected()) {
    reconnectMQTT();
  }
}

void EnviaEstadoOutputMQTT() {
  if (EstadoSaida == '1') {
    MQTT.publish(default_TOPICO_PUBLISH_1, "s|on");
  } else {
    MQTT.publish(default_TOPICO_PUBLISH_1, "s|off");
  }
}

void setup() {
  initSerial();
  InitOutput();
  initWiFi();
  initMQTT();
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  delay(2000);
}

void loop() {
  VerificaConexoesWiFIEMQTT();
  MQTT.loop();
  EnviaEstadoOutputMQTT();
  readSensors();
  delay(3000); // leitura a cada 3s
}