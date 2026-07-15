#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <SensirionI2cSps30.h>
#include <esp_sleep.h>
#include <math.h>

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr int LED_PIN = 2;

constexpr uint32_t WAKE_SETTLE_DELAY_MS = 1000;
constexpr uint32_t SPS30_WARMUP_DELAY_MS = 30000;
constexpr uint32_t DGS2_READ_TIMEOUT_MS = 1500;
constexpr uint8_t DGS2_MAX_RETRIES = 3;
constexpr uint8_t SPS30_MAX_RETRIES = 3;
constexpr uint32_t LORA_SETTLE_DELAY_MS = 1000;
constexpr uint64_t SLEEP_INTERVAL_US = 60ULL * 1000ULL * 1000ULL;

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
constexpr size_t MAX_LORA_PACKET_LENGTH = 255;

constexpr int SC16_CS_PIN = 4;
constexpr int SC16_RST_PIN = 14;
constexpr int SC16_IRQ_PIN = 5;
constexpr uint32_t SC16_XTAL_HZ = 14745600UL;
constexpr uint32_t DGS2_BAUD = 9600;
constexpr uint8_t SC16_CHANNEL_A = 0;
constexpr uint8_t SC16_CHANNEL_B = 1;

constexpr int O3_RX_PIN = 41;
constexpr int O3_TX_PIN = 42;
constexpr int CO_RX_PIN = 39;
constexpr int CO_TX_PIN = 40;

constexpr int32_t INVALID_INT_SENTINEL = -9999;
constexpr float INVALID_FLOAT_SENTINEL = -9999.0f;
constexpr const char* DEVICE_ID = "AQ01";

// Assumption based on the DGS2 wiring notes already present in the workspace.
constexpr const char* DGS2_SINGLE_MEASUREMENT_COMMAND = "\r";

RTC_DATA_ATTR uint32_t gSequenceNumber = 0;
RTC_DATA_ATTR uint32_t gWakeCycle = 0;

void deselectSpiDevices();

struct DGS2Reading {
  bool valid = false;
  String sensorName = "";
  String serialNumber = "";
  int32_t ppb = INVALID_INT_SENTINEL;
  float temperatureC = INVALID_FLOAT_SENTINEL;
  float humidityPercent = INVALID_FLOAT_SENTINEL;
};

struct SPS30Reading {
  bool valid = false;
  float pm1_0 = INVALID_FLOAT_SENTINEL;
  float pm2_5 = INVALID_FLOAT_SENTINEL;
  float pm10 = INVALID_FLOAT_SENTINEL;
};

struct EnvReading {
  bool valid = false;
  float temperatureC = INVALID_FLOAT_SENTINEL;
  float humidityPercent = INVALID_FLOAT_SENTINEL;
};

HardwareSerial o3Serial(1);
HardwareSerial coSerial(2);
SensirionI2cSps30 sps30;

DGS2Reading gCoReading;
DGS2Reading gO3Reading;
DGS2Reading gNo2Reading;
DGS2Reading gSo2Reading;
SPS30Reading gSps30Reading;
EnvReading gEnvReading;
bool gSc16Ready = false;
bool gSps30Ready = false;

class SC16IS752Bridge {
public:
  bool begin() {
    pinMode(SC16_CS_PIN, OUTPUT);
    digitalWrite(SC16_CS_PIN, HIGH);
    pinMode(SC16_RST_PIN, OUTPUT);
    digitalWrite(SC16_RST_PIN, HIGH);
    pinMode(SC16_IRQ_PIN, INPUT_PULLUP);

    hardwareReset();

    if (!scratchpadTest()) {
      Serial.println("[SC16] Scratchpad test failed.");
      return false;
    }

    configureUart(SC16_CHANNEL_A, DGS2_BAUD);
    configureUart(SC16_CHANNEL_B, DGS2_BAUD);
    Serial.println("[SC16] Initialized.");
    return true;
  }

  void flushRx(uint8_t channel) {
    uint32_t start = millis();
    while (rxAvailable(channel) > 0) {
      readByte(channel);
      if (millis() - start > 150) {
        break;
      }
    }
  }

  bool writeString(uint8_t channel, const char* text) {
    if (text == nullptr) {
      return false;
    }

    for (size_t i = 0; i < strlen(text); i++) {
      if (!writeByte(channel, static_cast<uint8_t>(text[i]))) {
        return false;
      }
      delay(2);
    }

    return true;
  }

  String readLine(uint8_t channel, uint32_t timeoutMs) {
    String line;
    uint32_t start = millis();

    while (millis() - start < timeoutMs) {
      while (rxAvailable(channel) > 0) {
        int value = readByte(channel);
        if (value < 0) {
          break;
        }

        char c = static_cast<char>(value);
        if (c == '\r' || c == '\n') {
          if (line.length() > 0) {
            line.trim();
            return line;
          }
        } else if (line.length() < 240) {
          line += c;
        }
      }
      yield();
    }

    line.trim();
    return line;
  }

private:
  static constexpr uint8_t REG_RHR = 0x00;
  static constexpr uint8_t REG_THR = 0x00;
  static constexpr uint8_t REG_IER = 0x01;
  static constexpr uint8_t REG_FCR = 0x02;
  static constexpr uint8_t REG_LCR = 0x03;
  static constexpr uint8_t REG_LSR = 0x05;
  static constexpr uint8_t REG_SPR = 0x07;
  static constexpr uint8_t REG_RXLVL = 0x09;
  static constexpr uint8_t REG_DLL = 0x00;
  static constexpr uint8_t REG_DLH = 0x01;
  static constexpr uint8_t REG_EFR = 0x02;
  static constexpr uint8_t LSR_THR_EMPTY = 0x20;

  void hardwareReset() {
    digitalWrite(SC16_RST_PIN, LOW);
    delay(20);
    digitalWrite(SC16_RST_PIN, HIGH);
    delay(100);
  }

  bool scratchpadTest() {
    writeRegister(SC16_CHANNEL_A, REG_SPR, 0x5A);
    delay(2);
    uint8_t a = readRegister(SC16_CHANNEL_A, REG_SPR);
    writeRegister(SC16_CHANNEL_A, REG_SPR, 0xA5);
    delay(2);
    uint8_t b = readRegister(SC16_CHANNEL_A, REG_SPR);
    return a == 0x5A && b == 0xA5;
  }

  void configureUart(uint8_t channel, uint32_t baud) {
    writeRegister(channel, REG_LCR, 0xBF);
    writeRegister(channel, REG_EFR, 0x10);
    writeRegister(channel, REG_LCR, 0x80);

    uint16_t divisor = static_cast<uint16_t>(SC16_XTAL_HZ / (baud * 16UL));
    writeRegister(channel, REG_DLL, divisor & 0xFF);
    writeRegister(channel, REG_DLH, (divisor >> 8) & 0xFF);

    writeRegister(channel, REG_LCR, 0x03);
    writeRegister(channel, REG_FCR, 0x07);
    delay(2);
    writeRegister(channel, REG_FCR, 0x01);
    writeRegister(channel, REG_IER, 0x00);
  }

  int rxAvailable(uint8_t channel) {
    return readRegister(channel, REG_RXLVL);
  }

  int readByte(uint8_t channel) {
    if (rxAvailable(channel) <= 0) {
      return -1;
    }
    return readRegister(channel, REG_RHR);
  }

  bool writeByte(uint8_t channel, uint8_t value) {
    uint32_t start = millis();
    while ((readRegister(channel, REG_LSR) & LSR_THR_EMPTY) == 0) {
      if (millis() - start > 100) {
        return false;
      }
      yield();
    }

    writeRegister(channel, REG_THR, value);
    return true;
  }

  uint8_t makeAddress(uint8_t channel, uint8_t reg, bool read) {
    return (read ? 0x80 : 0x00) | ((reg & 0x0F) << 3) | ((channel & 0x01) << 1);
  }

  void writeRegister(uint8_t channel, uint8_t reg, uint8_t value) {
    deselectSpiDevices();
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SC16_CS_PIN, LOW);
    SPI.transfer(makeAddress(channel, reg, false));
    SPI.transfer(value);
    digitalWrite(SC16_CS_PIN, HIGH);
    SPI.endTransaction();
  }

  uint8_t readRegister(uint8_t channel, uint8_t reg) {
    deselectSpiDevices();
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SC16_CS_PIN, LOW);
    SPI.transfer(makeAddress(channel, reg, true));
    uint8_t value = SPI.transfer(0x00);
    digitalWrite(SC16_CS_PIN, HIGH);
    SPI.endTransaction();
    return value;
  }
};

SC16IS752Bridge sc16;

bool isFiniteFloat(float value) {
  return !isnan(value) && isfinite(value);
}

bool isNumericString(const String& text) {
  if (text.length() == 0) {
    return false;
  }

  bool hasDigit = false;
  bool hasDecimal = false;

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
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

String formatIntOrSentinel(int32_t value) {
  return String(value);
}

String formatFloatOrSentinel(float value, uint8_t decimals) {
  if (!isFiniteFloat(value)) {
    return String(INVALID_FLOAT_SENTINEL, 1);
  }
  return String(value, static_cast<unsigned int>(decimals));
}

void deselectSpiDevices() {
  digitalWrite(SC16_CS_PIN, HIGH);
  digitalWrite(LORA_CS_PIN, HIGH);
}

void logRetry(const char* sensorName, uint8_t attempt, const char* reason) {
  Serial.print("[");
  Serial.print(sensorName);
  Serial.print("] Attempt ");
  Serial.print(attempt);
  Serial.print("/");
  Serial.print(DGS2_MAX_RETRIES);
  Serial.print(" failed: ");
  Serial.println(reason);
}

void flushHardwareSerial(HardwareSerial& port) {
  uint32_t start = millis();
  while (port.available()) {
    port.read();
    if (millis() - start > 150) {
      break;
    }
  }
}

String readLineFromHardwareSerial(HardwareSerial& port, uint32_t timeoutMs) {
  String line;
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (port.available()) {
      char c = static_cast<char>(port.read());
      if (c == '\r' || c == '\n') {
        if (line.length() > 0) {
          line.trim();
          return line;
        }
      } else if (line.length() < 240) {
        line += c;
      }
    }
    yield();
  }

  line.trim();
  return line;
}

bool parseDGS2Line(String line, DGS2Reading& out) {
  line.trim();
  if (line.length() == 0) {
    return false;
  }

  String fields[16];
  size_t fieldCount = 0;
  if (!splitCsvLine(line, fields, 16, fieldCount) || fieldCount < 7) {
    return false;
  }

  for (size_t i = 0; i < 7; i++) {
    if (fields[i].length() == 0) {
      return false;
    }
  }

  if (!isNumericString(fields[1]) || !isNumericString(fields[2]) || !isNumericString(fields[3])) {
    return false;
  }

  out.serialNumber = fields[0];
  out.ppb = fields[1].toInt();
  out.temperatureC = fields[2].toFloat();
  out.humidityPercent = fields[3].toFloat();

  // DGS2 modules in your logs report environment values as hundredths.
  if (fabsf(out.temperatureC) > 150.0f) {
    out.temperatureC /= 100.0f;
  }
  if (fabsf(out.humidityPercent) > 100.0f) {
    out.humidityPercent /= 100.0f;
  }

  if (!isFiniteFloat(out.temperatureC) || !isFiniteFloat(out.humidityPercent)) {
    return false;
  }

  if (out.temperatureC < -50.0f || out.temperatureC > 150.0f) {
    return false;
  }

  if (out.humidityPercent < 0.0f || out.humidityPercent > 100.0f) {
    return false;
  }

  out.valid = true;
  return true;
}

bool readDGS2Direct(HardwareSerial& port, const char* sensorName, DGS2Reading& out) {
  out = DGS2Reading();
  out.sensorName = sensorName;

  for (uint8_t attempt = 1; attempt <= DGS2_MAX_RETRIES; attempt++) {
    flushHardwareSerial(port);
    port.print(DGS2_SINGLE_MEASUREMENT_COMMAND);

    String line = readLineFromHardwareSerial(port, DGS2_READ_TIMEOUT_MS);
    Serial.print("[");
    Serial.print(sensorName);
    Serial.print("] Raw line: ");
    Serial.println(line);

    DGS2Reading parsed;
    parsed.sensorName = sensorName;
    if (line.length() == 0) {
      logRetry(sensorName, attempt, "timeout");
      continue;
    }

    if (!parseDGS2Line(line, parsed)) {
      logRetry(sensorName, attempt, "invalid parse");
      continue;
    }

    Serial.print("[");
    Serial.print(sensorName);
    Serial.print("] PPB=");
    Serial.print(parsed.ppb);
    Serial.print(" TEMP=");
    Serial.print(parsed.temperatureC, 1);
    Serial.print(" RH=");
    Serial.println(parsed.humidityPercent, 1);

    out = parsed;
    return true;
  }

  Serial.print("[");
  Serial.print(sensorName);
  Serial.println("] Failed after max retries.");
  return false;
}

bool readDGS2SC16(uint8_t channel, const char* sensorName, DGS2Reading& out) {
  out = DGS2Reading();
  out.sensorName = sensorName;

  for (uint8_t attempt = 1; attempt <= DGS2_MAX_RETRIES; attempt++) {
    sc16.flushRx(channel);
    if (!sc16.writeString(channel, DGS2_SINGLE_MEASUREMENT_COMMAND)) {
      logRetry(sensorName, attempt, "bridge write failed");
      continue;
    }

    String line = sc16.readLine(channel, DGS2_READ_TIMEOUT_MS);
    Serial.print("[");
    Serial.print(sensorName);
    Serial.print("] Raw line: ");
    Serial.println(line);

    DGS2Reading parsed;
    parsed.sensorName = sensorName;
    if (line.length() == 0) {
      logRetry(sensorName, attempt, "timeout");
      continue;
    }

    if (!parseDGS2Line(line, parsed)) {
      logRetry(sensorName, attempt, "invalid parse");
      continue;
    }

    Serial.print("[");
    Serial.print(sensorName);
    Serial.print("] PPB=");
    Serial.print(parsed.ppb);
    Serial.print(" TEMP=");
    Serial.print(parsed.temperatureC, 1);
    Serial.print(" RH=");
    Serial.println(parsed.humidityPercent, 1);

    out = parsed;
    return true;
  }

  Serial.print("[");
  Serial.print(sensorName);
  Serial.println("] Failed after max retries.");
  return false;
}

bool beginSps30() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
  Wire.setClock(I2C_FREQ_HZ);
  sps30.begin(Wire, SPS30_I2C_ADDRESS);

  Serial.println("[SPS30] I2C initialized.");
  return true;
}

bool ensureSps30MeasurementRunning() {
  if (!gSps30Ready) {
    return false;
  }

  for (uint8_t attempt = 1; attempt <= SPS30_MAX_RETRIES; attempt++) {
    int16_t error = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
    if (error == 0) {
      Serial.println("[SPS30] Continuous measurement active.");
      return true;
    }

    Serial.print("[SPS30] startMeasurement attempt ");
    Serial.print(attempt);
    Serial.print("/3 failed: ");
    Serial.println(error);

    uint16_t dataReady = 0;
    int16_t probeError = sps30.readDataReadyFlag(dataReady);
    if (probeError == 0) {
      Serial.println("[SPS30] Sensor is responsive. Assuming measurement is already running.");
      return true;
    }

    delay(100);
  }

  return false;
}

bool readSPS30(SPS30Reading& out) {
  out = SPS30Reading();

  for (uint8_t attempt = 1; attempt <= SPS30_MAX_RETRIES; attempt++) {
    uint16_t dataReady = 0;
    uint32_t start = millis();
    bool ready = false;

    while (millis() - start < DGS2_READ_TIMEOUT_MS) {
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

    Serial.print("[SPS30] PM1=");
    Serial.print(out.pm1_0, 2);
    Serial.print(" PM2.5=");
    Serial.print(out.pm2_5, 2);
    Serial.print(" PM10=");
    Serial.println(out.pm10, 2);
    return true;
  }

  Serial.println("[SPS30] Failed after max retries.");
  return false;
}

EnvReading averageValidDGS2Environment(DGS2Reading readings[], int count) {
  EnvReading env;
  float tempSum = 0.0f;
  float humiditySum = 0.0f;
  int validCount = 0;

  for (int i = 0; i < count; i++) {
    if (!readings[i].valid) {
      continue;
    }
    if (!isFiniteFloat(readings[i].temperatureC) || !isFiniteFloat(readings[i].humidityPercent)) {
      continue;
    }

    tempSum += readings[i].temperatureC;
    humiditySum += readings[i].humidityPercent;
    validCount++;
  }

  if (validCount == 0) {
    return env;
  }

  env.valid = true;
  env.temperatureC = tempSum / validCount;
  env.humidityPercent = humiditySum / validCount;
  return env;
}

uint16_t calculateCRC(String payloadWithoutCRC) {
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

uint8_t buildValidityMask() {
  uint8_t mask = 0;
  if (gCoReading.valid) {
    mask |= 1 << 0;
  }
  if (gO3Reading.valid) {
    mask |= 1 << 1;
  }
  if (gNo2Reading.valid) {
    mask |= 1 << 2;
  }
  if (gSo2Reading.valid) {
    mask |= 1 << 3;
  }
  if (gSps30Reading.valid) {
    mask |= 1 << 4;
  }
  if (gEnvReading.valid) {
    mask |= 1 << 5;
  }
  return mask;
}

String buildLoRaPacket() {
  String payloadWithoutCRC;
  payloadWithoutCRC.reserve(128);
  payloadWithoutCRC += DEVICE_ID;
  payloadWithoutCRC += ",";
  payloadWithoutCRC += String(gSequenceNumber);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(gCoReading.valid ? gCoReading.ppb : INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(gO3Reading.valid ? gO3Reading.ppb : INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(gNo2Reading.valid ? gNo2Reading.ppb : INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatIntOrSentinel(gSo2Reading.valid ? gSo2Reading.ppb : INVALID_INT_SENTINEL);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(gSps30Reading.valid ? gSps30Reading.pm1_0 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(gSps30Reading.valid ? gSps30Reading.pm2_5 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(gSps30Reading.valid ? gSps30Reading.pm10 : INVALID_FLOAT_SENTINEL, 2);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(gEnvReading.valid ? gEnvReading.temperatureC : INVALID_FLOAT_SENTINEL, 1);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += formatFloatOrSentinel(gEnvReading.valid ? gEnvReading.humidityPercent : INVALID_FLOAT_SENTINEL, 1);
  payloadWithoutCRC += ",";
  payloadWithoutCRC += String(buildValidityMask());

  uint16_t crc = calculateCRC(payloadWithoutCRC);
  String packet = payloadWithoutCRC + "," + crcToHex(crc);

  Serial.print("[PACKET] Validity mask: ");
  Serial.println(buildValidityMask());
  Serial.print("[PACKET] Payload before CRC: ");
  Serial.println(payloadWithoutCRC);
  Serial.print("[PACKET] CRC: ");
  Serial.println(crcToHex(crc));
  Serial.print("[PACKET] Final packet: ");
  Serial.println(packet);

  return packet;
}

bool beginLoRaRadio() {
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
  LoRa.enableCrc();
  Serial.println("[LORA] Initialized.");
  return true;
}

void transmitLoRaPacket(String packet) {
  if (packet.length() == 0 || packet.length() > MAX_LORA_PACKET_LENGTH) {
    Serial.print("[LORA] Packet length check failed: ");
    Serial.println(packet.length());
    return;
  }

  if (!beginLoRaRadio()) {
    return;
  }

  delay(LORA_SETTLE_DELAY_MS);

  int beginResult = LoRa.beginPacket();
  if (beginResult == 0) {
    Serial.println("[LORA] beginPacket failed.");
    LoRa.sleep();
    return;
  }

  LoRa.print(packet);
  int txResult = LoRa.endPacket();

  Serial.print("[LORA] Local transmit result: ");
  Serial.println(txResult == 1 ? "success" : "failed");
  LoRa.sleep();
  Serial.println("[LORA] Radio sleep entered.");
}

void enterSleep() {
  Serial.println("[SLEEP] Sleep start.");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_deep_sleep_start();
}

void beginBusesAndSerialPorts() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(SC16_CS_PIN, OUTPUT);
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(SC16_CS_PIN, HIGH);
  digitalWrite(LORA_CS_PIN, HIGH);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  o3Serial.begin(DGS2_BAUD, SERIAL_8N1, O3_RX_PIN, O3_TX_PIN);
  coSerial.begin(DGS2_BAUD, SERIAL_8N1, CO_RX_PIN, CO_TX_PIN);

  gSc16Ready = sc16.begin();
  gSps30Ready = beginSps30();
}

void waitForSps30WarmupWindow() {
  Serial.print("[APP] Waiting ");
  Serial.print(SPS30_WARMUP_DELAY_MS);
  Serial.println(" ms for SPS30 stabilization before reading sensors.");
  delay(SPS30_WARMUP_DELAY_MS);
}

void readDgs2SensorsFirst() {
  readDGS2Direct(coSerial, "CO", gCoReading);
  readDGS2Direct(o3Serial, "O3", gO3Reading);
  if (gSc16Ready) {
    readDGS2SC16(SC16_CHANNEL_B, "NO2", gNo2Reading);
    readDGS2SC16(SC16_CHANNEL_A, "SO2", gSo2Reading);
  } else {
    Serial.println("[SC16] Bridge unavailable. NO2 and SO2 marked invalid.");
  }

  DGS2Reading envInputs[4] = {gCoReading, gO3Reading, gNo2Reading, gSo2Reading};
  gEnvReading = averageValidDGS2Environment(envInputs, 4);

  if (gEnvReading.valid) {
    Serial.print("[ENV] Averaged TEMP=");
    Serial.print(gEnvReading.temperatureC, 1);
    Serial.print(" RH=");
    Serial.println(gEnvReading.humidityPercent, 1);
  } else {
    Serial.println("[ENV] No valid DGS2 temperature/humidity values.");
  }
}

void readSps30AfterDgs2() {
  if (gSps30Ready) {
    readSPS30(gSps30Reading);
  } else {
    Serial.println("[SPS30] Sensor unavailable. PM values marked invalid.");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  gWakeCycle++;
  gSequenceNumber++;

  Serial.println();
  Serial.println("=================================================");
  Serial.println("AiQuMo ESP32-S3 LoRa Transmitter");
  Serial.print("[APP] Wake cycle number: ");
  Serial.println(gWakeCycle);
  Serial.print("[APP] Sequence number: ");
  Serial.println(gSequenceNumber);
  Serial.println("=================================================");

  delay(WAKE_SETTLE_DELAY_MS);
  beginBusesAndSerialPorts();

  if (gSps30Ready) {
    gSps30Ready = ensureSps30MeasurementRunning();
  }

  waitForSps30WarmupWindow();
  readDgs2SensorsFirst();
  readSps30AfterDgs2();

  String packet = buildLoRaPacket();
  transmitLoRaPacket(packet);
  digitalWrite(LED_PIN, LOW);
  enterSleep();
}

void loop() {
}
