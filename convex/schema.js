import { defineSchema, defineTable } from "convex/server";
import { v } from "convex/values";

export default defineSchema({
  readings: defineTable({
    nodeId: v.string(),
    sequence: v.optional(v.number()),
    status: v.optional(v.string()),
    rawPayload: v.optional(v.string()),
    capturedAt: v.optional(v.number()),
    rssi: v.optional(v.number()),
    snr: v.optional(v.number()),
    so2Ppb: v.optional(v.number()),
    no2Ppb: v.optional(v.number()),
    o3Ppb: v.optional(v.number()),
    coPpb: v.optional(v.number()),
    temperatureC: v.optional(v.number()),
    humidityRh: v.optional(v.number()),
    pm1p0: v.optional(v.number()),
    pm2p5: v.optional(v.number()),
    pm10: v.optional(v.number()),
  }).index("by_capturedAt", ["capturedAt"]),
  dashboardState: defineTable({
    key: v.string(),
    resetAt: v.number(),
  }).index("by_key", ["key"]),
});
