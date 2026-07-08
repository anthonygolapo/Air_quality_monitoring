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
constexpr size_t EXPECTED_PACKET_FIELDS = 13;
constexpr size_t RX_QUEUE_SIZE = 4;
constexpr size_t MAX_RX_PAYLOAD_LENGTH = 255;

struct ReceivedPacket {
  char payload[MAX_RX_PAYLOAD_LENGTH + 1] = {0};
  uint16_t payloadLength = 0;
  int rssi = 0;
  float snr = 0.0f;
  bool ready = false;
};

volatile uint8_t gRxWriteIndex = 0;
volatile uint8_t gRxReadIndex = 0;
volatile uint8_t gRxCount = 0;
volatile uint32_t gRxDroppedPackets = 0;
ReceivedPacket gRxQueue[RX_QUEUE_SIZE];

void onReceiveLoRaPacket(int packetSize);

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

bool appendNumericJsonField(String& body, const char* key, const String& rawValue) {
  if (!isNumericString(rawValue)) {
    return false;
  }

  body += ",\"";
  body += key;
  body += "\":";
  body += rawValue;
  return true;
}

String toUpperTrimmed(String value) {
  value.trim();
  value.toUpperCase();
  return value;
}

uint16_t calculateCRC(const String& payloadWithoutCRC) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < payloadWithoutCRC.length(); i++) {
    crc ^= static_cast<uint16_t>(payloadWithoutCRC[i]) << 8;

    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

String crcToHex(uint16_t crc) {
  char buffer[5];
  snprintf(buffer, sizeof(buffer), "%04X", crc);
  return String(buffer);
}

String joinFields(const String fields[], size_t count) {
  String joined;

  for (size_t i = 0; i < count; i++) {
    if (i > 0) {
      joined += ",";
    }
    joined += fields[i];
  }

  return joined;
}

bool isHexCrc(const String& value) {
  if (value.length() != 4) {
    return false;
  }

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    bool ok = (c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f');
    if (!ok) {
      return false;
    }
  }

  return true;
}

bool isPrintablePayload(const String& payload) {
  for (size_t i = 0; i < payload.length(); i++) {
    char c = payload[i];
    if (c == '\r' || c == '\n' || c == '\t') {
      continue;
    }
    if (c < 32 || c > 126) {
      return false;
    }
  }

  return true;
}

bool validateCurrentPacket(const String& payload, String& reason) {
  if (payload.length() == 0) {
    reason = "empty payload";
    return false;
  }

  if (!isPrintablePayload(payload)) {
    reason = "non-printable payload";
    return false;
  }

  String fields[16];
  size_t fieldCount = 0;
  if (!splitCsvLine(payload, fields, 16, fieldCount) || fieldCount != EXPECTED_PACKET_FIELDS) {
    reason = "unexpected field count";
    return false;
  }

  if (!isNumericString(fields[1])) {
    reason = "sequence is not numeric";
    return false;
  }

  if (!isNumericString(fields[11])) {
    reason = "validity mask is not numeric";
    return false;
  }

  String payloadWithoutCrc = joinFields(fields, EXPECTED_PACKET_FIELDS - 1);
  String calculatedCrc = crcToHex(calculateCRC(payloadWithoutCrc));
  String receivedCrc = toUpperTrimmed(fields[12]);

  if (!isHexCrc(receivedCrc)) {
    reason = "crc field is not hex";
    return false;
  }

  if (receivedCrc != calculatedCrc) {
    reason = "crc mismatch";
    return false;
  }

  reason = "ok";
  return true;
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
  LoRa.onReceive(onReceiveLoRaPacket);
  LoRa.receive();

  Serial.println("[LORA] Receiver ready.");
  return true;
}

void onReceiveLoRaPacket(int packetSize) {
  if (packetSize <= 0) {
    LoRa.receive();
    return;
  }

  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  if (gRxCount >= RX_QUEUE_SIZE) {
    gRxDroppedPackets++;
    LoRa.receive();
    return;
  }

  uint8_t slot = gRxWriteIndex;
  uint16_t length = 0;
  while (LoRa.available() && length < MAX_RX_PAYLOAD_LENGTH) {
    gRxQueue[slot].payload[length++] = static_cast<char>(LoRa.read());
  }
  gRxQueue[slot].payload[length] = '\0';
  gRxQueue[slot].payloadLength = length;
  gRxQueue[slot].rssi = rssi;
  gRxQueue[slot].snr = snr;
  gRxQueue[slot].ready = true;

  gRxWriteIndex = static_cast<uint8_t>((gRxWriteIndex + 1) % RX_QUEUE_SIZE);
  gRxCount++;

  LoRa.receive();
}

String buildRequestBody(const String& rawPayload, int rssi, float snr) {
  String fields[16];
  size_t fieldCount = 0;

  String body = "{";
  body += "\"rawPayload\":\"" + escapeJson(rawPayload) + "\"";
  body += ",\"rssi\":" + String(rssi);
  body += ",\"snr\":" + String(snr, 2);

  if (splitCsvLine(rawPayload, fields, 16, fieldCount) &&
      fieldCount == EXPECTED_PACKET_FIELDS) {
    String payloadWithoutCrc = joinFields(fields, EXPECTED_PACKET_FIELDS - 1);
    String calculatedCrc = crcToHex(calculateCRC(payloadWithoutCrc));
    String receivedCrc = toUpperTrimmed(fields[12]);
    bool crcOk = isHexCrc(receivedCrc) && receivedCrc == calculatedCrc;

    body += ",\"deviceId\":\"" + escapeJson(fields[0]) + "\"";

    if (isNumericString(fields[1])) {
      body += ",\"sequence\":" + fields[1];
    }

    if (isNumericString(fields[11])) {
      body += ",\"validityMask\":" + fields[11];
    }

    appendNumericJsonField(body, "coPpb", fields[2]);
    appendNumericJsonField(body, "o3Ppb", fields[3]);
    appendNumericJsonField(body, "no2Ppb", fields[4]);
    appendNumericJsonField(body, "so2Ppb", fields[5]);
    appendNumericJsonField(body, "pm1_0", fields[6]);
    appendNumericJsonField(body, "pm2_5", fields[7]);
    appendNumericJsonField(body, "pm10", fields[8]);
    appendNumericJsonField(body, "temperatureC", fields[9]);
    appendNumericJsonField(body, "humidityPercent", fields[10]);

    body += ",\"crc\":\"" + escapeJson(receivedCrc) + "\"";
    body += ",\"crcOk\":";
    body += crcOk ? "true" : "false";
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

bool popReceivedPacket(ReceivedPacket& packet) {
  noInterrupts();
  if (gRxCount == 0) {
    interrupts();
    return false;
  }

  uint8_t slot = gRxReadIndex;
  packet = gRxQueue[slot];
  gRxQueue[slot].ready = false;
  gRxQueue[slot].payload[0] = '\0';
  gRxQueue[slot].payloadLength = 0;
  gRxReadIndex = static_cast<uint8_t>((gRxReadIndex + 1) % RX_QUEUE_SIZE);
  gRxCount--;
  interrupts();
  return true;
}

void loop() {
  ReceivedPacket packet;
  if (!popReceivedPacket(packet)) {
    static uint32_t lastDroppedLog = 0;
    if (gRxDroppedPackets > 0 && millis() - lastDroppedLog >= 5000UL) {
      lastDroppedLog = millis();
      Serial.print("[LORA] Dropped queued packets: ");
      Serial.println(gRxDroppedPackets);
    }
    delay(20);
    return;
  }

  digitalWrite(LED_PIN, HIGH);

  String payload = String(packet.payload);
  payload.trim();
  int rssi = packet.rssi;
  float snr = packet.snr;

  Serial.println();
  Serial.println("[LORA] Packet received");
  Serial.print("[LORA] Payload: ");
  Serial.println(payload);
  Serial.print("[LORA] RSSI: ");
  Serial.println(rssi);
  Serial.print("[LORA] SNR: ");
  Serial.println(snr, 2);

  String validationReason;
  if (!validateCurrentPacket(payload, validationReason)) {
    Serial.print("[LORA] Packet rejected: ");
    Serial.println(validationReason);
    digitalWrite(LED_PIN, LOW);
    return;
  }

  String fields[16];
  size_t fieldCount = 0;
  if (splitCsvLine(payload, fields, 16, fieldCount) && fieldCount == EXPECTED_PACKET_FIELDS) {
    String payloadWithoutCrc = joinFields(fields, EXPECTED_PACKET_FIELDS - 1);
    String calculatedCrc = crcToHex(calculateCRC(payloadWithoutCrc));
    String receivedCrc = toUpperTrimmed(fields[12]);

    Serial.print("[LORA] Device ID: ");
    Serial.println(fields[0]);
    Serial.print("[LORA] Sequence: ");
    Serial.println(fields[1]);
    Serial.print("[LORA] Validity mask: ");
    Serial.println(fields[11]);
    Serial.print("[LORA] CO ppb: ");
    Serial.println(fields[2]);
    Serial.print("[LORA] O3 ppb: ");
    Serial.println(fields[3]);
    Serial.print("[LORA] NO2 ppb: ");
    Serial.println(fields[4]);
    Serial.print("[LORA] SO2 ppb: ");
    Serial.println(fields[5]);
    Serial.print("[LORA] PM1.0: ");
    Serial.println(fields[6]);
    Serial.print("[LORA] PM2.5: ");
    Serial.println(fields[7]);
    Serial.print("[LORA] PM10: ");
    Serial.println(fields[8]);
    Serial.print("[LORA] Temperature C: ");
    Serial.println(fields[9]);
    Serial.print("[LORA] Humidity %: ");
    Serial.println(fields[10]);
    Serial.print("[LORA] CRC received: ");
    Serial.println(receivedCrc);
    Serial.print("[LORA] CRC calculated: ");
    Serial.println(calculatedCrc);
    Serial.print("[LORA] CRC result: ");
    Serial.println((isHexCrc(receivedCrc) && receivedCrc == calculatedCrc) ? "PASS" : "FAIL");
  } else {
    Serial.println("[LORA] Packet format does not match current transmitter layout.");
  }

  if (payload.length() == 0) {
    Serial.println("[LORA] Empty payload skipped.");
    digitalWrite(LED_PIN, LOW);
    return;
  }

  if (postToConvex(payload, rssi, snr)) {
    Serial.println("[APP] Forwarded to Convex successfully.");
  } else {
    Serial.println("[APP] Failed to forward packet.");
  }

  digitalWrite(LED_PIN, LOW);
}
