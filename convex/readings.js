import { v } from "convex/values";
import { mutation, query } from "./_generated/server";

const EXPECTED_PACKET_MS = 60 * 1000;
const INVALID_SENTINEL = -9999;

function parseMaybeNumber(value) {
  if (value === null || value === undefined || value === "" || value === "nan") {
    return undefined;
  }

  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed === INVALID_SENTINEL) {
    return undefined;
  }

  return parsed;
}

function hasValidityBit(validityMask, bitIndex) {
  return Number.isInteger(validityMask) && (validityMask & (1 << bitIndex)) !== 0;
}

function normalizeWithValidity(value, validityMask, bitIndex) {
  if (!hasValidityBit(validityMask, bitIndex)) {
    return undefined;
  }

  return value;
}

function parseStatusFromPayload(rawPayload) {
  if (!rawPayload) {
    return undefined;
  }

  const match = rawPayload.match(/STAT=([A-Z0-9]+)/);
  return match ? match[1] : undefined;
}

function formatValidityStatus(validityMask) {
  if (!Number.isInteger(validityMask)) {
    return undefined;
  }

  const labels = [
    validityMask & (1 << 0) ? "CO" : "co?",
    validityMask & (1 << 1) ? "O3" : "o3?",
    validityMask & (1 << 2) ? "NO2" : "no2?",
    validityMask & (1 << 3) ? "SO2" : "so2?",
    validityMask & (1 << 4) ? "PM" : "pm?",
    validityMask & (1 << 5) ? "ENV" : "env?",
  ];

  return labels.join(" ");
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

function isFiniteNumber(value) {
  return typeof value === "number" && Number.isFinite(value);
}

function computePacketGapSummary(readings) {
  if (readings.length < 2) {
    return {
      totalMissingPackets: 0,
      gapEvents: 0,
      longestGapPackets: 0,
      resetsDetected: 0,
      latestGap: null,
    };
  }

  let totalMissingPackets = 0;
  let gapEvents = 0;
  let longestGapPackets = 0;
  let resetsDetected = 0;
  let latestGap = null;

  for (let index = 1; index < readings.length; index += 1) {
    const previous = readings[index - 1];
    const current = readings[index];
    const previousTimestamp = previous.capturedAt ?? previous._creationTime;
    const currentTimestamp = current.capturedAt ?? current._creationTime;
    const previousSequence = previous.sequence;
    const currentSequence = current.sequence;

    let missingPackets = 0;
    let basis = "time";

    if (isFiniteNumber(previousSequence) && isFiniteNumber(currentSequence)) {
      if (currentSequence > previousSequence) {
        missingPackets = Math.max(currentSequence - previousSequence - 1, 0);
        basis = "sequence";
      } else if (currentSequence < previousSequence) {
        resetsDetected += 1;
        continue;
      } else {
        continue;
      }
    } else {
      const elapsedMs = currentTimestamp - previousTimestamp;
      const inferredIntervals = Math.max(1, Math.round(elapsedMs / EXPECTED_PACKET_MS));
      missingPackets = Math.max(inferredIntervals - 1, 0);
    }

    if (missingPackets <= 0) {
      continue;
    }

    totalMissingPackets += missingPackets;
    gapEvents += 1;
    longestGapPackets = Math.max(longestGapPackets, missingPackets);
    latestGap = {
      fromSequence: previousSequence ?? null,
      toSequence: currentSequence ?? null,
      fromTimestamp: previousTimestamp,
      toTimestamp: currentTimestamp,
      missingPackets,
      basis,
    };
  }

  return {
    totalMissingPackets,
    gapEvents,
    longestGapPackets,
    resetsDetected,
    latestGap,
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
    validityMask: v.optional(v.number()),
    crc: v.optional(v.string()),
    crcOk: v.optional(v.boolean()),
    so2Ppb: v.optional(v.union(v.number(), v.string())),
    no2Ppb: v.optional(v.union(v.number(), v.string())),
    o3Ppb: v.optional(v.union(v.number(), v.string())),
    coPpb: v.optional(v.union(v.number(), v.string())),
    temperatureC: v.optional(v.union(v.number(), v.string())),
    humidityRh: v.optional(v.union(v.number(), v.string())),
    humidityPercent: v.optional(v.union(v.number(), v.string())),
    pm1p0: v.optional(v.union(v.number(), v.string())),
    pm2p5: v.optional(v.union(v.number(), v.string())),
    pm1_0: v.optional(v.union(v.number(), v.string())),
    pm2_5: v.optional(v.union(v.number(), v.string())),
    pm10: v.optional(v.union(v.number(), v.string())),
  },
  handler: async (ctx, args) => {
    const parsedPayload = parsePayload(args.rawPayload);
    const parsedValidityMask = parseMaybeNumber(args.validityMask);
    const derivedStatus =
      args.status ??
      parsedPayload.status ??
      parseStatusFromPayload(args.rawPayload) ??
      formatValidityStatus(parsedValidityMask);

    const so2Ppb = normalizeWithValidity(
      parseMaybeNumber(args.so2Ppb) ?? parsedPayload.so2Ppb,
      parsedValidityMask,
      3
    );
    const no2Ppb = normalizeWithValidity(
      parseMaybeNumber(args.no2Ppb) ?? parsedPayload.no2Ppb,
      parsedValidityMask,
      2
    );
    const o3Ppb = normalizeWithValidity(
      parseMaybeNumber(args.o3Ppb) ?? parsedPayload.o3Ppb,
      parsedValidityMask,
      1
    );
    const coPpb = normalizeWithValidity(
      parseMaybeNumber(args.coPpb) ?? parsedPayload.coPpb,
      parsedValidityMask,
      0
    );
    const temperatureC = normalizeWithValidity(
      parseMaybeNumber(args.temperatureC) ?? parsedPayload.temperatureC,
      parsedValidityMask,
      5
    );
    const humidityRh = normalizeWithValidity(
      parseMaybeNumber(args.humidityRh) ??
        parseMaybeNumber(args.humidityPercent) ??
        parsedPayload.humidityRh,
      parsedValidityMask,
      5
    );
    const pm1p0 = normalizeWithValidity(
      parseMaybeNumber(args.pm1p0) ??
        parseMaybeNumber(args.pm1_0) ??
        parsedPayload.pm1p0,
      parsedValidityMask,
      4
    );
    const pm2p5 = normalizeWithValidity(
      parseMaybeNumber(args.pm2p5) ??
        parseMaybeNumber(args.pm2_5) ??
        parsedPayload.pm2p5,
      parsedValidityMask,
      4
    );
    const pm10 = normalizeWithValidity(
      parseMaybeNumber(args.pm10) ?? parsedPayload.pm10,
      parsedValidityMask,
      4
    );

    const doc = {
      nodeId: args.nodeId ?? parsedPayload.nodeId ?? "NODE-001",
      sequence: args.sequence ?? parsedPayload.sequence,
      status: derivedStatus,
      rawPayload: args.rawPayload,
      capturedAt: args.capturedAt ?? Date.now(),
      rssi: args.rssi,
      snr: args.snr,
      validityMask: parsedValidityMask,
      crc: args.crc,
      crcOk: args.crcOk,
      so2Ppb,
      no2Ppb,
      o3Ppb,
      coPpb,
      temperatureC,
      humidityRh,
      pm1p0,
      pm2p5,
      pm10,
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

export const getPacketGapSummary = query({
  args: {},
  handler: async (ctx) => {
    const readings = await ctx.db
      .query("readings")
      .withIndex("by_capturedAt")
      .order("asc")
      .collect();

    return computePacketGapSummary(readings);
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
