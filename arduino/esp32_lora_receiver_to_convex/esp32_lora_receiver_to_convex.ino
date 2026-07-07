#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LoRa.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* CONVEX_INGEST_URL = "https://pastel-anteater-608.convex.site/ingest";

constexpr int LED_PIN = 2;
constexpr int SPI_MOSI_PIN = 11;
constexpr int SPI_MISO_PIN = 13;
constexpr int SPI_SCK_PIN = 12;
constexpr int LORA_NSS_PIN = 10;
constexpr int LORA_RESET_PIN = 18;
constexpr int LORA_DIO0_PIN = 17;
constexpr long LORA_FREQUENCY = 915E6;
constexpr int LORA_SPREADING_FACTOR = 7;
constexpr long LORA_SIGNAL_BANDWIDTH = 125E3;
constexpr int LORA_CODING_RATE_DENOMINATOR = 5;
constexpr int LORA_SYNC_WORD = 0x12;

constexpr uint32_t WIFI_RETRY_MS = 500;
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
constexpr uint32_t HTTP_TIMEOUT_MS = 15000;

String escapeJson(const String& input) {
  String output;
  output.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];

    switch (c) {
      case '\\':
        output += "\\\\";
        break;
      case '"':
        output += "\\\"";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        output += c;
        break;
    }
  }

  return output;
}

bool splitCsvLine(const String& raw, String fields[], size_t maxFields, size_t& fieldCount) {
  fieldCount = 0;
  int start = 0;

  for (int i = 0; i <= raw.length(); i++) {
    if (i == raw.length() || raw[i] == ',') {
      if (fieldCount >= maxFields) {
        return false;
      }

      fields[fieldCount] = raw.substring(start, i);
      fields[fieldCount].trim();
      fieldCount++;
      start = i + 1;
    }
  }

  return fieldCount > 0;
}

bool isNumericString(const String& s) {
  if (s.length() == 0) {
    return false;
  }

  bool hasDigit = false;
  bool hasDecimal = false;

  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];

    if (c >= '0' && c <= '9') {
      hasDigit = true;
      continue;
    }

    if ((c == '-' || c == '+') && i == 0) {
      continue;
    }

    if (c == '.' && !hasDecimal) {
      hasDecimal = true;
      continue;
    }

    return false;
  }

  return hasDigit;
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_RETRY_MS);
    Serial.print(".");

    if (millis() - start >= WIFI_TIMEOUT_MS) {
      Serial.println();
      Serial.println("[WIFI] Connection timeout.");
      return false;
    }
  }

  Serial.println();
  Serial.print("[WIFI] Connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool beginLoRa() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(LORA_NSS_PIN, OUTPUT);
  digitalWrite(LORA_NSS_PIN, HIGH);

  pinMode(LORA_RESET_PIN, OUTPUT);
  digitalWrite(LORA_RESET_PIN, HIGH);

  pinMode(LORA_DIO0_PIN, INPUT);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  LoRa.setPins(LORA_NSS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("[LORA] Initialization failed.");
    return false;
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE_DENOMINATOR);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  LoRa.receive();

  Serial.println("[LORA] Receiver ready.");
  return true;
}

String buildRequestBody(const String& rawPayload, int rssi, float snr) {
  String fields[16];
  size_t fieldCount = 0;

  String body = "{";
  body += "\"rawPayload\":\"" + escapeJson(rawPayload) + "\"";
  body += ",\"rssi\":" + String(rssi);
  body += ",\"snr\":" + String(snr, 2);

  if (splitCsvLine(rawPayload, fields, 16, fieldCount) &&
      fieldCount >= 3 &&
      fields[0] == "ENV1") {
    body += ",\"nodeId\":\"" + escapeJson(fields[1]) + "\"";

    if (isNumericString(fields[2])) {
      body += ",\"sequence\":" + fields[2];
    }
  }

  body += "}";
  return body;
}

bool postToConvex(const String& rawPayload, int rssi, float snr) {
  if (!ensureWiFi()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(client, CONVEX_INGEST_URL)) {
    Serial.println("[HTTP] Could not begin request.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  String body = buildRequestBody(rawPayload, rssi, snr);

  Serial.println("[HTTP] Posting packet to Convex...");
  int statusCode = http.POST(body);
  String response = http.getString();
  http.end();

  Serial.print("[HTTP] Status: ");
  Serial.println(statusCode);
  Serial.print("[HTTP] Response: ");
  Serial.println(response);

  if (statusCode < 0) {
    Serial.print("[HTTP] Error: ");
    Serial.println(http.errorToString(statusCode));
  }

  return statusCode >= 200 && statusCode < 300;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=======================================");
  Serial.println("ESP32 LoRa Receiver -> Convex");
  Serial.println("=======================================");

  if (!beginLoRa()) {
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(1000);
    }
  }

  ensureWiFi();
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize <= 0) {
    delay(50);
    return;
  }

  digitalWrite(LED_PIN, HIGH);

  String payload;
  while (LoRa.available()) {
    payload += static_cast<char>(LoRa.read());
  }

  payload.trim();

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  Serial.println();
  Serial.println("[LORA] Packet received");
  Serial.print("[LORA] Payload: ");
  Serial.println(payload);
  Serial.print("[LORA] RSSI: ");
  Serial.println(rssi);
  Serial.print("[LORA] SNR: ");
  Serial.println(snr, 2);

  if (payload.length() == 0) {
    Serial.println("[LORA] Empty payload skipped.");
    digitalWrite(LED_PIN, LOW);
    LoRa.receive();
    return;
  }

  if (postToConvex(payload, rssi, snr)) {
    Serial.println("[APP] Forwarded to Convex successfully.");
  } else {
    Serial.println("[APP] Failed to forward packet.");
  }

  digitalWrite(LED_PIN, LOW);
  LoRa.receive();
}
