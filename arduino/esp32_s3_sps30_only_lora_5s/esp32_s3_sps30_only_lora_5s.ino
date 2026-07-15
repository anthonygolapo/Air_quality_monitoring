#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <SensirionI2cSps30.h>
#include <math.h>

constexpr uint32_t SERIAL_BAUD = 115200;

constexpr int I2C_SDA_PIN = 8;
constexpr int I2C_SCL_PIN = 9;
constexpr uint8_t SPS30_I2C_ADDRESS = 0x69;
constexpr uint32_t I2C_FREQ_HZ = 100000;

constexpr int SPI_MOSI_PIN = 11;
constexpr int SPI_MISO_PIN = 13;
constexpr int SPI_SCK_PIN = 12;

constexpr int LORA_CS_PIN = 10;
constexpr int LORA_RST_PIN = 18;
constexpr int LORA_DIO0_PIN = 17;
constexpr long LORA_FREQUENCY = 915E6;
constexpr int LORA_SPREADING_FACTOR = 7;
constexpr long LORA_SIGNAL_BANDWIDTH = 125E3;
constexpr int LORA_CODING_RATE_DENOMINATOR = 5;
constexpr int LORA_SYNC_WORD = 0x12;
constexpr int LORA_TX_POWER_DBM = 17;

constexpr uint32_t SPS30_WARMUP_DELAY_MS = 30000;
constexpr uint32_t SPS30_READ_INTERVAL_MS = 5000;
constexpr uint32_t SPS30_DATA_READY_TIMEOUT_MS = 1500;
constexpr uint8_t SPS30_MAX_RETRIES = 3;
constexpr uint32_t LORA_SETTLE_DELAY_MS = 1000;
constexpr size_t MAX_LORA_PACKET_LENGTH = 255;

constexpr int32_t INVALID_INT_SENTINEL = -9999;
constexpr float INVALID_FLOAT_SENTINEL = -9999.0f;
constexpr const char* DEVICE_ID = "AQ01";

uint32_t gSequenceNumber = 0;
SensirionI2cSps30 sps30;

struct Sps30Reading {
  bool valid = false;
  float pm1_0 = INVALID_FLOAT_SENTINEL;
  float pm2_5 = INVALID_FLOAT_SENTINEL;
  float pm10 = INVALID_FLOAT_SENTINEL;
};

bool isFiniteFloat(float value) {
  return !isnan(value) && isfinite(value);
}

String formatFloatOrSentinel(float value, uint8_t decimals) {
  if (!isFiniteFloat(value) || value == INVALID_FLOAT_SENTINEL) {
    return String(INVALID_FLOAT_SENTINEL, 1);
  }
  return String(value, static_cast<unsigned int>(decimals));
}

String formatIntOrSentinel(int32_t value) {
  return String(value);
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

uint8_t buildValidityMask(const Sps30Reading& reading) {
  uint8_t mask = 0;
  if (reading.valid) {
    mask |= 1 << 4;
  }
  return mask;
}

bool beginSps30() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
  Wire.setClock(I2C_FREQ_HZ);
  sps30.begin(Wire, SPS30_I2C_ADDRESS);
  Serial.println("[SPS30] I2C initialized.");
  return true;
}

bool startSps30Measurement() {
  for (uint8_t attempt = 1; attempt <= SPS30_MAX_RETRIES; attempt++) {
    int16_t error = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
    if (error == 0) {
      Serial.println("[SPS30] Continuous measurement started.");
      return true;
    }

    Serial.print("[SPS30] startMeasurement attempt ");
    Serial.print(attempt);
    Serial.print("/3 failed: ");
    Serial.println(error);
    delay(100);
  }

  return false;
}

bool readSps30(Sps30Reading& out) {
  out = Sps30Reading();

  for (uint8_t attempt = 1; attempt <= SPS30_MAX_RETRIES; attempt++) {
    uint16_t dataReady = 0;
    uint32_t start = millis();
    bool ready = false;

    while (millis() - start < SPS30_DATA_READY_TIMEOUT_MS) {
      int16_t error = sps30.readDataReadyFlag(dataReady);
      if (error != 0) {
        Serial.print("[SPS30] Data-ready read failed on attempt ");
        Serial.println(attempt);
        dataReady = 0;
        break;
      }

      if (dataReady) {
        ready = true;
        break;
      }

      delay(50);
    }

    if (!ready) {
      Serial.print("[SPS30] Attempt ");
      Serial.print(attempt);
      Serial.println("/3 failed: timeout");
      continue;
    }

    float mc1p0 = NAN;
    float mc2p5 = NAN;
    float mc4p0 = NAN;
    float mc10p0 = NAN;
    float nc0p5 = NAN;
    float nc1p0 = NAN;
    float nc2p5 = NAN;
    float nc4p0 = NAN;
    float nc10p0 = NAN;
    float typicalParticleSize = NAN;

    int16_t error = sps30.readMeasurementValuesFloat(
      mc1p0,
      mc2p5,
      mc4p0,
      mc10p0,
      nc0p5,
      nc1p0,
      nc2p5,
      nc4p0,
      nc10p0,
      typicalParticleSize
    );

    if (error != 0) {
      Serial.print("[SPS30] Attempt ");
      Serial.print(attempt);
      Serial.println("/3 failed: read error");
      continue;
    }

    if (!isFiniteFloat(mc1p0) || !isFiniteFloat(mc2p5) || !isFiniteFloat(mc10p0)) {
      Serial.print("[SPS30] Attempt ");
      Serial.print(attempt);
      Serial.println("/3 failed: invalid numeric values");
      continue;
    }

    out.valid = true;
    out.pm1_0 = mc1p0;
    out.pm2_5 = mc2p5;
    out.pm10 = mc10p0;
    return true;
  }

  return false;
}

bool beginLoRaRadio() {
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(LORA_CS_PIN, HIGH);
  pinMode(LORA_RST_PIN, OUTPUT);
  digitalWrite(LORA_RST_PIN, HIGH);
  pinMode(LORA_DIO0_PIN, INPUT);

  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("[LORA] Initialization failed.");
    return false;
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE_DENOMINATOR);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setTxPower(LORA_TX_POWER_DBM);
  Serial.println("[LORA] Initialized.");
  return true;
}

String buildLoRaPacket(const Sps30Reading& reading) {
  uint8_t validityMask = buildValidityMask(reading);

  String payloadWithoutCRC;
  payloadWithoutCRC.reserve(128);
  payloadWithoutCRC += DEVICE_ID;
  payloadWithoutCRC += ",";
  payloadWithoutCRC += String(gSequenceNumber);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(reading.valid ? reading.pm1_0 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(reading.valid ? reading.pm2_5 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(reading.valid ? reading.pm10 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(INVALID_FLOAT_SENTINEL, 1);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(INVALID_FLOAT_SENTINEL, 1);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += String(validityMask);

  uint16_t crc = calculateCRC(payloadWithoutCRC);
  String packet = payloadWithoutCRC + "," + crcToHex(crc);

  Serial.print("[PACKET] Final packet: ");
  Serial.println(packet);
  return packet;
}

void transmitLoRaPacket(const String& packet) {
  if (packet.length() == 0 || packet.length() > MAX_LORA_PACKET_LENGTH) {
    Serial.print("[LORA] Packet length check failed: ");
    Serial.println(packet.length());
    return;
  }

  delay(LORA_SETTLE_DELAY_MS);

  if (LoRa.beginPacket() == 0) {
    Serial.println("[LORA] beginPacket failed.");
    return;
  }

  LoRa.print(packet);
  int result = LoRa.endPacket();
  Serial.print("[LORA] Transmit result: ");
  Serial.println(result == 1 ? "success" : "failure");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  Serial.println();
  Serial.println("=================================================");
  Serial.println("ESP32-S3 SPS30 LoRa Transmitter");
  Serial.println("=================================================");

  beginSps30();
  beginLoRaRadio();

  if (!startSps30Measurement()) {
    Serial.println("[SPS30] Failed to start measurement. Check wiring and library.");
    return;
  }

  Serial.print("[APP] Waiting ");
  Serial.print(SPS30_WARMUP_DELAY_MS);
  Serial.println(" ms for SPS30 stabilization.");
  delay(SPS30_WARMUP_DELAY_MS);
}

void loop() {
  Sps30Reading reading;
  gSequenceNumber++;

  if (!readSps30(reading)) {
    Serial.println("[SPS30] Failed to read measurement.");
  } else {
    Serial.print("[SPS30] PM1.0=");
    Serial.print(reading.pm1_0, 2);
    Serial.print(" ug/m3, PM2.5=");
    Serial.print(reading.pm2_5, 2);
    Serial.print(" ug/m3, PM10=");
    Serial.print(reading.pm10, 2);
    Serial.println(" ug/m3");
  }

  String packet = buildLoRaPacket(reading);
  transmitLoRaPacket(packet);
  delay(SPS30_READ_INTERVAL_MS);
}
