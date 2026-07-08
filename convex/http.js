import { httpRouter } from "convex/server";
import { httpAction } from "./_generated/server";

const http = httpRouter();

http.route({
  path: "/ingest",
  method: "POST",
  handler: httpAction(async (ctx, request) => {
    const body = await request.json();

    const id = await ctx.runMutation("readings:ingestReading", {
      nodeId: body.nodeId ?? body.deviceId ?? "NODE-001",
      sequence: body.sequence,
      status: body.status,
      rawPayload: body.rawPayload,
      capturedAt: body.capturedAt,
      rssi: body.rssi,
      snr: body.snr,
      validityMask: body.validityMask,
      crc: body.crc,
      crcOk: body.crcOk,
      so2Ppb: body.so2Ppb,
      no2Ppb: body.no2Ppb,
      o3Ppb: body.o3Ppb,
      coPpb: body.coPpb,
      temperatureC: body.temperatureC,
      humidityRh: body.humidityRh ?? body.humidityPercent,
      pm1p0: body.pm1p0 ?? body.pm1_0,
      pm2p5: body.pm2p5 ?? body.pm2_5,
      pm10: body.pm10,
    });

    return new Response(JSON.stringify({ ok: true, id }), {
      headers: { "Content-Type": "application/json" },
      status: 200,
    });
  }),
});

export default http;
