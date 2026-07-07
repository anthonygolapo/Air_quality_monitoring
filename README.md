# Air Quality Dashboard

Simple real-time dashboard for LoRa air-quality packets forwarded by an ESP32 into Convex.

## What this includes

- A `readings` table in Convex for live sensor packets
- A mutation and HTTP endpoint to insert readings
- A React dashboard that updates in real time with Convex subscriptions
- An ESP32 receiver sketch that forwards LoRa packets to Convex

## Expected sensor fields

This dashboard expects data shaped like your Arduino payload:

`ENV1,NODE-001,SEQ,SO2,NO2,O3,CO,T,H,PM1,PM25,PM10,CRC,STAT=...`

The frontend reads these normalized fields:

- `nodeId`
- `sequence`
- `status`
- `rawPayload`
- `capturedAt`
- `so2Ppb`
- `no2Ppb`
- `o3Ppb`
- `coPpb`
- `temperatureC`
- `humidityRh`
- `pm1p0`
- `pm2p5`
- `pm10`

## Run locally

1. Install packages:

```powershell
npm install
```

2. Create your local env file:

```powershell
Copy-Item .env.example .env.local
```

3. Put your Convex deployment URL in `.env.local`:

```env
VITE_CONVEX_URL=https://your-deployment.convex.cloud
```

4. Start Convex in one terminal:

```powershell
npx convex dev
```

5. Start the dashboard in another terminal:

```powershell
npm run dev
```

## Deploy to production

Deploy the Convex backend and build the frontend against the production Convex URL with:

```powershell
npx.cmd convex deploy --yes --cmd "npm.cmd run build" --cmd-url-env-var-name VITE_CONVEX_URL
```

After deploy:

- Convex will push your backend to the production deployment
- `dist/` will be built using the production Convex URL
- upload the contents of `dist/` to your static host of choice

Current production Convex URL:

`https://pastel-anteater-608.convex.cloud`

For HTTP ingest from the ESP32, use the production site URL:

`https://pastel-anteater-608.convex.site/ingest`

Examples of static hosts:

- Netlify
- Vercel
- GitHub Pages

If your ESP32 receiver posts directly to Convex, update its ingest URL to the production `.convex.site` URL after production deploy.

## ESP32 -> Convex

If your ESP32 already writes directly to Convex, you can keep that flow and just make sure it inserts documents with the fields above.

If you want a simple HTTP target instead, post JSON to:

`/ingest`

Example JSON body:

```json
{
  "nodeId": "NODE-001",
  "sequence": 12,
  "rawPayload": "ENV1,NODE-001,12,5,9,18,121,29.4,71.2,7.1,12.4,21.8,ABCD,STAT=N1S1O1C1P1",
  "so2Ppb": 5,
  "no2Ppb": 9,
  "o3Ppb": 18,
  "coPpb": 121,
  "temperatureC": 29.4,
  "humidityRh": 71.2,
  "pm1p0": 7.1,
  "pm2p5": 12.4,
  "pm10": 21.8
}
```

You can also send only the raw LoRa packet and let Convex parse it:

```json
{
  "rawPayload": "ENV1,NODE-001,12,5,9,18,121,29.4,71.2,7.1,12.4,21.8,ABCD,STAT=N1S1O1C1P1"
}
```

## ESP32 receiver sketch

The ready-to-upload receiver code is here:

[arduino/esp32_lora_receiver_to_convex/esp32_lora_receiver_to_convex.ino](C:/Users/ENVI-COMM/Desktop/Airquality/arduino/esp32_lora_receiver_to_convex/esp32_lora_receiver_to_convex.ino:1)

It:

- Listens for the LoRa packet from your sensor node
- Connects to Wi-Fi
- Posts the payload to Convex `/ingest`
- Includes RSSI and SNR for link monitoring
