# CP 5 - Vinheria IoT – PoC 
## Engenharia de Software - 1ESPZ - FIAP

> **Versão focada em ThingSpeak** (FIWARE/Orion indisponível no período da entrega).
> Prova de conceito completa: **ESP32 (Wokwi) → ThingSpeak (Cloud)** com gráficos em tempo real, verificação via **Postman/cURL**, código reproduzível e estrutura organizada.

---

## 👥 Integrantes

* **Camila de Mendonça da Silva** - RM: 565491
* **Davi Alves dos Santos** - RM: 
*  **Rafael Joda** - RM: 561939
* **Luis Miguel** RM: 561232
* **Diego Zandonadi** RM: 561488

---

## 🗺️ Visão Geral da Arquitetura

**Sensores (DHT22, LDR, HC-SR04) → ESP32 (Wokwi) → HTTP → ThingSpeak Channel (field1..field4) → Gráficos**

* O ESP32 envia leituras por **HTTP GET** (ou **POST**) para o endpoint `/update` do **ThingSpeak**.
* Cada sensor é mapeado para um **Field** do canal (1 a 4).
* Visualização em tempo real pela página do canal (Private/Public View).
* Validação de integração feita com **Postman** e **cURL**.

---

## 📁 Estrutura do Repositório

```
vinheria-iot-poc-thingspeak/
├─ README.md                              # este documento
├─ docs/
│  ├─ prints-postman/
│  │  ├─ 01_update-get-200.png            # POST/GET /update → 200/202
│  │  ├─ 02_update-post-200.png
│  │  └─ 03_feeds-json.png                # GET /feeds.json com últimas leituras
│  ├─ prints-wokwi/
│  │  ├─ 01_wokwi-running.png             # simulação rodando
│  │  └─ 02_serial-http-2xx.png           # Serial com HTTP 2xx
│  └─ prints-thingspeak/
│     ├─ 01_channel-fields.png            # fields 1..4 configurados
│     └─ 02_charts-updating.png           # gráficos atualizando
├─ firmware/
│  └─ esp32-wokwi-thingspeak/
│     └─ main.cpp                         # código final do ESP32 (Wokwi)
├─ deploy/
│  └─ scripts/
│     ├─ send-sample.sh                   # exemplo de envio por shell (cURL)
│     └─ healthcheck.sh                   # verificação básica do canal
└─ .env.example                           # variáveis (API KEY, CHANNEL ID)
```

---

## 🧩 Mapeamento dos Campos (ThingSpeak)

| Sensor       | Variável no código | Field ThingSpeak |
| ------------ | ------------------ | ---------------- |
| Temperatura  | `t` (°C)           | `field1`         |
| Umidade      | `h` (%)            | `field2`         |
| Luminosidade | `luminosidade` (%) | `field3`         |
| Distância    | `distance` (cm)    | `field4`         |

---

## 🔧 Pré-requisitos

* Conta no **ThingSpeak** (MathWorks).
* **Channel** criado com **4 fields** (Temperature, Humidity, Luminosity, Distance).
* **Write API Key** do canal (ex.: `KGC2JE6OQJFPZ6SR`).
* **Wokwi** (simulador online): [https://wokwi.com](https://wokwi.com) (projeto **ESP32**).
* **Postman** (ou cURL) para validar endpoints e gerar prints.

---

## 💻 Código Final (ESP32 — Wokwi → ThingSpeak)

> Cole em `firmware/esp32-wokwi-thingspeak/main.cpp` e no Wokwi (ESP32).
> Ajuste **`apiKey`** antes de rodar.

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ---------- PINAGEM ----------
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
const char* apiKey   = "SUA_WRITE_API_KEY_AQUI"; // <-- ALTERE

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" conectado!");
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) return NAN;
  return (duration * 0.0343f) / 2.0f; // cm
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Falha ao ler o DHT22 (NaN).");
      delay(2000);
      return;
    }

    int ldrRaw = analogRead(LDR_PIN);                 // 0..4095
    float luminosidade = (ldrRaw / 4095.0f) * 100.0f; // %

    float distance = readDistanceCm();
    float distanceToSend = isnan(distance) ? 0.0f : distance;

    Serial.println("----- Leitura -----");
    Serial.print("Temp (°C): ");  Serial.println(t, 1);
    Serial.print("Umid  (%): ");  Serial.println(h, 0);
    Serial.print("Lumi  (%): ");  Serial.println(luminosidade, 0);
    Serial.print("Dist (cm): ");  Serial.println(distanceToSend, 1);

    // Envio por GET (plano free)
    HTTPClient http;
    String url = String(server) + "/update?api_key=" + apiKey +
                 "&field1=" + String(t, 1) +
                 "&field2=" + String(h, 0) +
                 "&field3=" + String(luminosidade, 0) +
                 "&field4=" + String(distanceToSend, 1);

    http.begin(url);
    int code = http.GET();
    Serial.print("HTTP status: "); Serial.println(code);
    http.end();

  } else {
    Serial.println("WiFi desconectado. Reconectando...");
    WiFi.reconnect();
  }

  // ThingSpeak (free): 15 s entre updates por chave
  delay(15000);
}
```

### Pinagem (Wokwi)

* **DHT22:** VCC→3.3V, GND→GND, DATA→GPIO15
* **LDR:** divisor com 10k; nó central → **GPIO34**
* **HC-SR04:** VCC→5V, GND→GND, TRIG→GPIO5, ECHO→GPIO18

  * *(mundo real: divisor de tensão no ECHO para 3.3V; no Wokwi pode direto)*

---

## 📸 Prints exigidos (onde gerar)

* `docs/prints-wokwi/01_wokwi-running.png` → Wokwi em execução
* `docs/prints-wokwi/02_serial-http-2xx.png` → Serial Monitor com “HTTP status: 200/202”
* `docs/prints-thingspeak/01_channel-fields.png` → Channel com 4 fields criados
* `docs/prints-thingspeak/02_charts-updating.png` → Gráficos atualizando (Private/Public View)
* `docs/prints-postman/01_update-get-200.png` → Postman chamando **GET /update** (200/202)
* `docs/prints-postman/02_update-post-200.png` → Postman chamando **POST /update.json** (200)
* `docs/prints-postman/03_feeds-json.png` → Postman chamando **GET /channels/{id}/feeds.json**

---

## 🧪 Validação com Postman (integração IoT)

> Substitua `WRITE_API_KEY` e `CHANNEL_ID`.

**1) Enviar por GET (igual ao firmware)**

```
GET https://api.thingspeak.com/update?api_key=WRITE_API_KEY&field1=25.3&field2=61&field3=45&field4=13.2
```

**Esperado:** `200 OK` (ou `202 Accepted`) e corpo com o **Entry ID** inserido.

**2) Enviar por POST (JSON)**

```
POST https://api.thingspeak.com/update.json
Headers:
  Content-Type: application/json
Body:
{
  "api_key": "WRITE_API_KEY",
  "field1": 26.1,
  "field2": 60,
  "field3": 52,
  "field4": 12.8
}
```

**Esperado:** `200 OK` e Entry ID no corpo.

**3) Consultar últimas leituras (JSON)**

```
GET https://api.thingspeak.com/channels/CHANNEL_ID/feeds.json?results=5
```

**Esperado:** `200 OK` com array `feeds` contendo seus `field1..field4`.

> Gere prints no Postman de cada etapa e salve em `docs/prints-postman/`.

---

## 🧪 Resultados da PoC

* **Coleta e envio confiáveis** (a cada 15 s) de **temperatura, umidade, luminosidade e distância**.
* **Gráficos em tempo real** no ThingSpeak (Private/Public View).
* **Integração IoT validada** com Postman/cURL (`/update` e `/feeds.json`).
* **Reprodutibilidade** confirmada por passos de configuração e código final.

---

## 🔁 Replicabilidade (passo a passo)

1. **Criar canal** no ThingSpeak com 4 fields.
2. **Anotar** `WRITE_API_KEY` e `CHANNEL_ID`.
3. **Clonar** este repositório:

   ```bash
   git clone <URL-DO-REPO>
   cd vinheria-iot-poc-thingspeak
   cp .env.example .env   # opcional
   ```
4. **Abrir Wokwi** → ESP32 → colar `firmware/esp32-wokwi-thingspeak/main.cpp`.
5. **Editar** a constante `apiKey` no código.
6. **Conectar sensores** conforme pinagem e clicar **Run**.
7. **Conferir Serial**: “HTTP status: 200/202”.
8. **Abrir canal** no ThingSpeak (Private View) e verificar gráficos.
9. **Validar no Postman**: `/update` (GET/POST) e `/feeds.json`.
10. **Capturar prints** e salvar nas pastas `docs/prints-*` conforme lista.

---

## 🧰 Scripts de Deploy / Teste (opc.)

> Coloque em `deploy/scripts/` (exemplos abaixo).
> **Obs.:** São utilitários para teste/validação via shell (sem firmware).

**`send-sample.sh`**

```bash
#!/usr/bin/env bash
# Exemplo: ./send-sample.sh WRITE_API_KEY 27.5 63 40 14.1
KEY="$1"; T="$2"; H="$3"; L="$4"; D="$5"
curl -s "https://api.thingspeak.com/update?api_key=${KEY}&field1=${T}&field2=${H}&field3=${L}&field4=${D}"
echo
```

**`healthcheck.sh`**

```bash
#!/usr/bin/env bash
# Exemplo: ./healthcheck.sh CHANNEL_ID
CHAN="$1"
curl -s "https://api.thingspeak.com/channels/${CHAN}/feeds.json?results=1" | jq .
```

> Dê permissão: `chmod +x deploy/scripts/*.sh`.

---

## 📄 Observações

* Esta versão cumpre os objetivos de **coleta → transmissão → visualização** em nuvem, incluindo **validação via Postman**.
* Quando um **Context Broker FIWARE** estiver novamente disponível, basta trocar o **endpoint** e o **formato** (Ultralight). O restante do fluxo (coleta e logs) permanece igual.

---
