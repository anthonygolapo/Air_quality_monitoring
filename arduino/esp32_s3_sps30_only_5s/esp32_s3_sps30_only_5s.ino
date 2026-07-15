#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cSps30.h>

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t I2C_FREQ_HZ = 100000;
constexpr int I2C_SDA_PIN = 8;
constexpr int I2C_SCL_PIN = 9;
constexpr uint8_t SPS30_I2C_ADDRESS = 0x69;

constexpr uint32_t SPS30_WARMUP_DELAY_MS = 30000;
constexpr uint32_t SPS30_READ_INTERVAL_MS = 5000;
constexpr uint32_t SPS30_DATA_READY_TIMEOUT_MS = 1500;
constexpr uint8_t SPS30_MAX_RETRIES = 3;

SensirionI2cSps30 sps30;

struct Sps30Reading {
  bool valid = false;
  float pm1_0 = NAN;
  float pm2_5 = NAN;
  float pm10 = NAN;
};

bool isFiniteFloat(float value) {
  return !isnan(value) && isfinite(value);
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

void printReading(const Sps30Reading& reading) {
  if (!reading.valid) {
    Serial.println("[SPS30] Reading invalid.");
    return;
  }

  Serial.print("[SPS30] PM1.0=");
  Serial.print(reading.pm1_0, 2);
  Serial.print(" ug/m3, PM2.5=");
  Serial.print(reading.pm2_5, 2);
  Serial.print(" ug/m3, PM10=");
  Serial.print(reading.pm10, 2);
  Serial.println(" ug/m3");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  Serial.println();
  Serial.println("=================================================");
  Serial.println("ESP32-S3 SPS30 Continuous Reader");
  Serial.println("=================================================");

  beginSps30();

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
  if (readSps30(reading)) {
    printReading(reading);
  } else {
    Serial.println("[SPS30] Failed to read measurement.");
  }

  delay(SPS30_READ_INTERVAL_MS);
}
