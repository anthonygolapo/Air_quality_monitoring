# ESP32 Arduino Sketches

## Transmitter

Use [esp32_s3_aiqumo_lora_transmitter/esp32_s3_aiqumo_lora_transmitter.ino](C:/Users/ENVI-COMM/Desktop/Airquality/arduino/esp32_s3_aiqumo_lora_transmitter/esp32_s3_aiqumo_lora_transmitter.ino:1) on your ESP32-S3 AiQuMo LoRa transmitter node.

This sketch:

- wakes every 60 seconds
- reads CO, O3, NO2, SO2, SPS30 PM, and averaged DGS2 temperature/RH
- builds one fixed CSV packet with a validity mask and CRC
- transmits over SX1276 LoRa
- puts LoRa and ESP32 back to sleep

Install these Arduino libraries before uploading:

- `LoRa`
- `Sensirion I2C SPS30`
- Built-in ESP32 `SPI`
- Built-in ESP32 `Wire`

Important transmitter note:

- The DGS2 single-measurement command is currently set to `"\r"` in the sketch because that is the command pattern already present in the local hardware notes. If your DGS2 firmware expects a different trigger command, update `DGS2_SINGLE_MEASUREMENT_COMMAND` before uploading.

## Receiver

Use [esp32_lora_receiver_to_convex/esp32_lora_receiver_to_convex.ino](C:/Users/ENVI-COMM/Desktop/Airquality/arduino/esp32_lora_receiver_to_convex/esp32_lora_receiver_to_convex.ino:1) on the ESP32 that receives LoRa and forwards packets to Convex over Wi-Fi.

## Before uploading

Update these values in the sketch:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `CONVEX_INGEST_URL`

Your Convex HTTP URL should look like:

`https://your-deployment.convex.site/ingest`

## Libraries

Install these Arduino libraries:

- `LoRa`
- Built-in ESP32 `WiFi`
- Built-in ESP32 `HTTPClient`

## Important

Make sure the receiver LoRa settings match the transmitter:

- Frequency: `915E6`
- Spreading factor: `7`
- Bandwidth: `125E3`
- Coding rate denominator: `5`
- Sync word: `0x12`

## Flow

1. LoRa packet arrives on ESP32.
2. ESP32 reads the raw payload.
3. ESP32 sends JSON to Convex `/ingest`.
4. Convex stores the reading.
5. The React dashboard updates in real time.
