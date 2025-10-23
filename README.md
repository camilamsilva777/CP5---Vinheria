# CP 5 - Vinheria IoT – PoC

## Engenharia de Software - 1ESPZ - FIAP

> Prova de conceito completa: **ESP32 (Wokwi) → MQTT Broker → IoT Agent (Ultralight) → Orion Context Broker** com consulta e testes via **Postman/cURL**, código reproduzível e estrutura organizada.

---

## 👥 Integrantes

* **Camila de Mendonça da Silva** - RM: 565491
* **Davi Alves dos Santos** - RM: 566279
* **Rafael Joda** - RM: 561939
* **Luis Miguel** - RM: 561232
* **Diego Zandonadi** - RM: 561488

---

## 🗺️ Visão Geral da Arquitetura

**Sensores (DHT22, LDR, HC-SR04) → ESP32 (Wokwi) → MQTT → IoT Agent (Ultralight/MQTT) → Orion Context Broker → Postman**

* O ESP32 publica leituras por **MQTT** nos tópicos `…/attrs/*`.
* O **IoT Agent (Ultralight)** consome essas publicações e **atualiza atributos** da entidade no **Orion** (NGSI v2).
* **Postman** é usado para **consultar/validar** a entidade no Orion (GET /v2/entities).
* (Opcional) **Comandos** podem ser enviados do Orion → IoT Agent → ESP32 (o firmware já ouve `/cmd` e liga/desliga um pino).

---

## 📁 Estrutura do Repositório

```
vinheria-iot-poc-orion/
├─ README.md                              # este documento
├─ docs/
│  ├─ prints-wokwi/
│  │  ├─ 01_wokwi-running.png             # simulação rodando
│  │  └─ 02_serial-mqtt-ok.png            # Serial OK com broker MQTT
│  ├─ prints-postman/
│  │  ├─ 01_entity-create.png             # (se aplicável) criação do device/entity
│  │  ├─ 02_entity-get.png                # consulta no Orion
│  │  └─ 03_entity-attrs-updated.png      # atributos atualizados
├─ firmware/
│  └─ main.cpp                            # código final do ESP32 (Wokwi) - MQTT/Ultralight
├─ deploy/
│  └─ scripts/
│     ├─ send-ultralight.sh               # publica MQTT (simulado) p/ testes
│     └─ query-orion.sh                   # consulta entidade no Orion
└─ .env.example                           # variáveis (BROKER, ORION, DEVICE_ID, TOPICS)
```

---

## 🧩 Mapeamento (Atributos → Orion via IoT Agent)

| Sensor       | Variável (código) | Tópico MQTT                      | Atributo no Orion | Unidade      |   |
| ------------ | ----------------- | -------------------------------- | ----------------- | ------------ | - |
| Temperatura  | `temperatura`     | `/TEF/lamp011/attrs/t`           | `temperature`     | °C           |   |
| Umidade      | `umidade`         | `/TEF/lamp011/attrs/h`           | `humidity`        | %            |   |
| Luminosidade | `luminosity`      | `/TEF/lamp011/attrs/l`           | `luminosity`      | %            |   |
| Distância    | `distance`        | `/TEF/lamp011/attrs/u`           | `distance`        | cm           |   |
| Estado LED   | (saída D4)        | `/TEF/lamp011/attrs` (payload `s | on/off`)          | `s` (status) | — |

> **Comandos** recebidos no tópico `/TEF/lamp011/cmd` com payload contendo `@on|` ou `@off|` (o firmware já interpreta e alterna o pino `default_D4`).
> O **IoT Agent** deve estar configurado para mapear `t,h,l,u,s` → atributos da entidade `lamp011` (ou `VINHERIA001`, conforme cadastro).

---

## 🔧 Pré-requisitos

* **Broker MQTT** acessível (ex.: Mosquitto).
* **FIWARE IoT Agent Ultralight (MQTT)** configurado e apontando para o **Orion**.
* **Orion Context Broker** ativo (NGSI v2).
* **Postman** (ou cURL) para validações.
* **Wokwi** (ESP32) – rede padrão: `Wokwi-GUEST`.

> **Endpoints de exemplo (ajuste para o seu ambiente)**
>
> * Broker MQTT: `172.24.240.1:1883`
> * IoT Agent: `http://<iot-agent-host>:4041`
> * Orion: `http://<orion-host>:1026`

---

## 💻 Código Final (ESP32 → MQTT → Orion)

> Cole em `firmware/main.cpp`. Ajuste **BROKER**, **TOPICS** e **DEVICE** se necessário.

```cpp
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
```

> **Destaques do firmware:**
>
> * Publica `t,h,l,u` em `/TEF/lamp011/attrs/*`.
> * Publica estado `s|on/off` em `/TEF/lamp011/attrs`.
> * Recebe comandos em `/TEF/lamp011/cmd` com `@on|` / `@off|`.
> * Leitura DHT22 (T/UR), LDR (0–100%), HC-SR04 (cm).

---

## 🔌 Pinagem (Wokwi)

* **DHT22:** VCC→3.3V, GND→GND, DATA→GPIO15
* **LDR:** divisor com 10kΩ; nó central → **GPIO34**
* **HC-SR04:** VCC→5V, GND→GND, **TRIG→GPIO5**, **ECHO→GPIO18**
* **LED (default_D4):** GPIO2 (on-board)

> 💡 No hardware real, **ECHO** precisa de **divisor de tensão** para 3.3 V.

---

## 📸 Imagens & Evidências

* `docs/prints-wokwi/01_wokwi-running.png` → Simulação rodando
* `docs/prints-wokwi/02_serial-mqtt-ok.png` → Conectado no broker MQTT
* `docs/prints-postman/02_entity-get.png` → Consulta no Orion com atributos atualizados
* `docs/prints-postman/03_entity-attrs-updated.png` → JSON NGSI v2 com `temperature`, `humidity`, `luminosity`, `distance`

---

## 🧪 Validação com Postman (Orion)

> **Pré-condição:** O **IoT Agent** já registrou o **device** e mapeamentos (service, servicepath, deviceId, atributos `t,h,l,u,s`).
> Assim que o ESP32 publicar em MQTT, o IoT Agent **atualiza** a entidade no Orion.

**1) Consultar entidade (NGSI v2)**

```
GET http://<orion-host>:1026/v2/entities/<ENTITY_ID>
```

**Resposta esperada (exemplo):**

```json
{
  "id": "lamp011",
  "type": "WineCellar",
  "temperature": { "value": 24.5, "type": "Number" },
  "humidity":    { "value": 62.1, "type": "Number" },
  "luminosity":  { "value": 48,   "type": "Number" },
  "distance":    { "value": 12.4, "type": "Number" },
  "s":           { "value": "on", "type": "Text" }
}
```

**2) (Opcional) Enviar comando para o dispositivo**

> Forma recomendada quando o device está **cadastrado no IoT Agent** como *command*:

```
POST http://<iot-agent-host>:4041/iot/devices/<DEVICE_ID>/commands
Content-Type: application/json

{ "on": "" }
```

> **Alternativa para teste rápido (direto no MQTT):** publicar em `/TEF/lamp011/cmd` a string `@on|` ou `@off|` (o firmware já entende).

---

## 🔁 Replicabilidade (passo a passo)

1. **Confirmar ambiente**: Broker MQTT, IoT Agent Ultralight (MQTT) e Orion ativos.
2. **Cadastrar device** no IoT Agent (mapeando `t,h,l,u,s` → Orion).
3. **Abrir Wokwi**, carregar `firmware/main.cpp`, conferir **IP do broker** e **TOPICS**.
4. **Run** no Wokwi e observar **Serial** (conexão Wi-Fi, MQTT e publicações).
5. No **Postman**, usar `GET /v2/entities/<ENTITY_ID>` no Orion e validar atributos.
6. (Opcional) Enviar **comando** via IoT Agent (ou publish MQTT) e observar o LED/PINO `default_D4`.
---

## 🧰 Scripts (opcionais)

**`deploy/scripts/query-orion.sh`**

```bash
#!/usr/bin/env bash
ORION=${ORION:-http://localhost:1026}
ENTITY_ID=${1:-lamp011}
curl -s "${ORION}/v2/entities/${ENTITY_ID}" | jq .
```

**`deploy/scripts/send-ultralight.sh`** (exemplo didático – publicar direto no broker)

```bash
#!/usr/bin/env bash
# Requer mosquitto-clients instalados
BROKER=${BROKER:-172.24.240.1}
mosquitto_pub -h $BROKER -t /TEF/lamp011/attrs/t -m "25.0"
mosquitto_pub -h $BROKER -t /TEF/lamp011/attrs/h -m "60"
mosquitto_pub -h $BROKER -t /TEF/lamp011/attrs/l -m "45"
mosquitto_pub -h $BROKER -t /TEF/lamp011/attrs/u -m "12.3"
```

---

## 🧪 Resultados da PoC

* **Coleta confiável** (a cada 3 s) de **temperatura, umidade, luminosidade e distância**.
* **Publicação MQTT** e **ingestão via IoT Agent** com atualização no **Orion**.
* **Validação Postman** via `GET /v2/entities/<id>` (NGSI v2).
* **Comandos** do backend para o dispositivo (Orion/IoT Agent → MQTT → ESP32).

---
