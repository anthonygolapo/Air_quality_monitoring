import { v } from "convex/values";
import { mutation, query } from "./_generated/server";

function parseMaybeNumber(value) {
  if (value === null || value === undefined || value === "" || value === "nan") {
    return undefined;
  }

  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : undefined;
}

function parseStatusFromPayload(rawPayload) {
  if (!rawPayload) {
    return undefined;
  }

  const match = rawPayload.match(/STAT=([A-Z0-9]+)/);
  return match ? match[1] : undefined;
}

function parsePayload(rawPayload) {
  if (!rawPayload) {
    return {};
  }

  const [body, statusSection] = rawPayload.split(",STAT=");
  const parts = body.split(",");

  if (parts.length < 13 || parts[0] !== "ENV1") {
    return {
      status: statusSection,
    };
  }

  return {
    nodeId: parts[1],
    sequence: parseMaybeNumber(parts[2]),
    so2Ppb: parseMaybeNumber(parts[3]),
    no2Ppb: parseMaybeNumber(parts[4]),
    o3Ppb: parseMaybeNumber(parts[5]),
    coPpb: parseMaybeNumber(parts[6]),
    temperatureC: parseMaybeNumber(parts[7]),
    humidityRh: parseMaybeNumber(parts[8]),
    pm1p0: parseMaybeNumber(parts[9]),
    pm2p5: parseMaybeNumber(parts[10]),
    pm10: parseMaybeNumber(parts[11]),
    status: statusSection,
  };
}

export const ingestReading = mutation({
  args: {
    nodeId: v.optional(v.string()),
    sequence: v.optional(v.number()),
    status: v.optional(v.string()),
    rawPayload: v.optional(v.string()),
    capturedAt: v.optional(v.number()),
    rssi: v.optional(v.number()),
    snr: v.optional(v.number()),
    so2Ppb: v.optional(v.union(v.number(), v.string())),
    no2Ppb: v.optional(v.union(v.number(), v.string())),
    o3Ppb: v.optional(v.union(v.number(), v.string())),
    coPpb: v.optional(v.union(v.number(), v.string())),
    temperatureC: v.optional(v.union(v.number(), v.string())),
    humidityRh: v.optional(v.union(v.number(), v.string())),
    pm1p0: v.optional(v.union(v.number(), v.string())),
    pm2p5: v.optional(v.union(v.number(), v.string())),
    pm10: v.optional(v.union(v.number(), v.string())),
  },
  handler: async (ctx, args) => {
    const parsedPayload = parsePayload(args.rawPayload);

    const doc = {
      nodeId: args.nodeId ?? parsedPayload.nodeId ?? "NODE-001",
      sequence: args.sequence ?? parsedPayload.sequence,
      status:
        args.status ??
        parsedPayload.status ??
        parseStatusFromPayload(args.rawPayload),
      rawPayload: args.rawPayload,
      capturedAt: args.capturedAt ?? Date.now(),
      rssi: args.rssi,
      snr: args.snr,
      so2Ppb: parseMaybeNumber(args.so2Ppb) ?? parsedPayload.so2Ppb,
      no2Ppb: parseMaybeNumber(args.no2Ppb) ?? parsedPayload.no2Ppb,
      o3Ppb: parseMaybeNumber(args.o3Ppb) ?? parsedPayload.o3Ppb,
      coPpb: parseMaybeNumber(args.coPpb) ?? parsedPayload.coPpb,
      temperatureC:
        parseMaybeNumber(args.temperatureC) ?? parsedPayload.temperatureC,
      humidityRh:
        parseMaybeNumber(args.humidityRh) ?? parsedPayload.humidityRh,
      pm1p0: parseMaybeNumber(args.pm1p0) ?? parsedPayload.pm1p0,
      pm2p5: parseMaybeNumber(args.pm2p5) ?? parsedPayload.pm2p5,
      pm10: parseMaybeNumber(args.pm10) ?? parsedPayload.pm10,
    };

    return await ctx.db.insert("readings", doc);
  },
});

export const getLatest = query({
  args: {},
  handler: async (ctx) => {
    const latest = await ctx.db.query("readings").order("desc").first();
    return latest ?? null;
  },
});

export const listRecent = query({
  args: {
    limit: v.optional(v.number()),
  },
  handler: async (ctx, args) => {
    const limit = Math.min(args.limit ?? 20, 2000);
    return await ctx.db.query("readings").order("desc").take(limit);
  },
});

export const getWarmupReset = query({
  args: {},
  handler: async (ctx) => {
    const state = await ctx.db
      .query("dashboardState")
      .withIndex("by_key", (query) => query.eq("key", "dgs2Warmup"))
      .first();

    return state ?? null;
  },
});

export const resetWarmupTimer = mutation({
  args: {},
  handler: async (ctx) => {
    const now = Date.now();
    const existing = await ctx.db
      .query("dashboardState")
      .withIndex("by_key", (query) => query.eq("key", "dgs2Warmup"))
      .first();

    if (existing) {
      await ctx.db.patch(existing._id, { resetAt: now });
      return existing._id;
    }

    return await ctx.db.insert("dashboardState", {
      key: "dgs2Warmup",
      resetAt: now,
    });
  },
});
