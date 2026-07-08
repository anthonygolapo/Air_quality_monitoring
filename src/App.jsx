import { useEffect, useMemo, useRef, useState } from "react";
import { useMutation, useQuery } from "convex/react";
import { api } from "../convex/_generated/api";

const overviewCards = [
  { key: "pm2p5", label: "PM2.5", unit: "ug/m3", accent: "accent-coral" },
  { key: "pm10", label: "PM10", unit: "ug/m3", accent: "accent-gold" },
  { key: "pm1p0", label: "PM1.0", unit: "ug/m3", accent: "accent-sky" },
  { key: "coPpb", label: "CO", unit: "ppb", accent: "accent-plum" },
  { key: "o3Ppb", label: "O3", unit: "ppb", accent: "accent-sunset" },
  { key: "no2Ppb", label: "NO2", unit: "ppb", accent: "accent-ember" },
  { key: "so2Ppb", label: "SO2", unit: "ppb", accent: "accent-steel" },
];

const gasSeries = [
  { key: "no2Ppb", label: "NO2", color: "#34d5ff" },
  { key: "so2Ppb", label: "SO2", color: "#ffd447" },
  { key: "coPpb", label: "CO", color: "#d36bff" },
  { key: "o3Ppb", label: "O3", color: "#73f0c0" },
];

const rangeOptions = [
  { key: "hour", label: "Hour", windowMs: 60 * 60 * 1000, bucketMs: 5 * 60 * 1000 },
  { key: "today", label: "Today", windowMs: null, bucketMs: 30 * 60 * 1000 },
  { key: "day", label: "24 Hour", windowMs: 24 * 60 * 60 * 1000, bucketMs: 60 * 60 * 1000 },
  { key: "week", label: "Day", windowMs: 7 * 24 * 60 * 60 * 1000, bucketMs: 24 * 60 * 60 * 1000 },
  { key: "month", label: "Month", windowMs: 30 * 24 * 60 * 60 * 1000, bucketMs: 3 * 24 * 60 * 60 * 1000 },
];

const stabilityConfig = [
  {
    key: "no2Ppb",
    label: "NO2",
    unit: "ppb",
    mode: "dgs2",
    minPoints: 4,
    minWindowMs: 30 * 60 * 1000,
    targetWindowMs: 60 * 60 * 1000,
    goodThreshold: 30,
    acceptableThreshold: 60,
    healthAverageWindowMs: 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 53, tone: "good" },
      { label: "Moderate", max: 100, tone: "moderate" },
      { label: "Sensitive Groups", max: 360, tone: "sensitive" },
      { label: "Unhealthy", max: 649, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  {
    key: "so2Ppb",
    label: "SO2",
    unit: "ppb",
    mode: "dgs2",
    minPoints: 4,
    minWindowMs: 30 * 60 * 1000,
    targetWindowMs: 60 * 60 * 1000,
    goodThreshold: 30,
    acceptableThreshold: 60,
    healthAverageWindowMs: 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 35, tone: "good" },
      { label: "Moderate", max: 75, tone: "moderate" },
      { label: "Sensitive Groups", max: 185, tone: "sensitive" },
      { label: "Unhealthy", max: 304, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  {
    key: "coPpb",
    label: "CO",
    unit: "ppb",
    mode: "dgs2",
    minPoints: 4,
    minWindowMs: 30 * 60 * 1000,
    targetWindowMs: 60 * 60 * 1000,
    goodThreshold: 200,
    acceptableThreshold: 400,
    healthAverageWindowMs: 8 * 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 4400, tone: "good" },
      { label: "Moderate", max: 9400, tone: "moderate" },
      { label: "Sensitive Groups", max: 12400, tone: "sensitive" },
      { label: "Unhealthy", max: 15400, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  {
    key: "o3Ppb",
    label: "O3",
    unit: "ppb",
    mode: "dgs2",
    minPoints: 4,
    minWindowMs: 30 * 60 * 1000,
    targetWindowMs: 60 * 60 * 1000,
    goodThreshold: 20,
    acceptableThreshold: 40,
    healthAverageWindowMs: 8 * 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 54, tone: "good" },
      { label: "Moderate", max: 70, tone: "moderate" },
      { label: "Sensitive Groups", max: 85, tone: "sensitive" },
      { label: "Unhealthy", max: 105, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  {
    key: "pm2p5",
    label: "PM2.5",
    unit: "ug/m3",
    mode: "basic",
    minPoints: 4,
    tolerance: 8,
    healthAverageWindowMs: 24 * 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 9.0, tone: "good" },
      { label: "Moderate", max: 35.4, tone: "moderate" },
      { label: "Sensitive Groups", max: 55.4, tone: "sensitive" },
      { label: "Unhealthy", max: 125.4, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  {
    key: "pm10",
    label: "PM10",
    unit: "ug/m3",
    mode: "basic",
    minPoints: 4,
    tolerance: 15,
    healthAverageWindowMs: 24 * 60 * 60 * 1000,
    healthBreakpoints: [
      { label: "Good", max: 54, tone: "good" },
      { label: "Moderate", max: 154, tone: "moderate" },
      { label: "Sensitive Groups", max: 254, tone: "sensitive" },
      { label: "Unhealthy", max: 354, tone: "unhealthy" },
      { label: "Very Unhealthy", max: Infinity, tone: "very" },
    ],
  },
  { key: "temperatureC", label: "Temperature", unit: "C", mode: "basic", minPoints: 4, tolerance: 1.2 },
  { key: "humidityRh", label: "Humidity", unit: "%", mode: "basic", minPoints: 4, tolerance: 4 },
];

const STALE_PACKET_MS = 60 * 60 * 1000;
const DGS2_WARMUP_MS = 24 * 60 * 60 * 1000;

function formatValue(value, unit, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(value)) {
    return "--";
  }

  return `${Number(value).toFixed(digits)} ${unit}`;
}

function formatCompactValue(value, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(value)) {
    return "--";
  }

  return Number(value).toFixed(digits);
}

function formatDate(timestamp) {
  if (!timestamp) {
    return "--";
  }

  return new Intl.DateTimeFormat("en-US", {
    dateStyle: "medium",
    timeStyle: "medium",
  }).format(new Date(timestamp));
}

function formatAxisLabel(timestamp, rangeKey) {
  const date = new Date(timestamp);

  if (rangeKey === "hour" || rangeKey === "today") {
    return new Intl.DateTimeFormat("en-US", {
      hour: "numeric",
      minute: "2-digit",
    }).format(date);
  }

  if (rangeKey === "day") {
    return new Intl.DateTimeFormat("en-US", {
      hour: "numeric",
    }).format(date);
  }

  return new Intl.DateTimeFormat("en-US", {
    month: "short",
    day: "numeric",
  }).format(date);
}

function formatAge(ms) {
  if (ms <= 0) {
    return "just now";
  }

  const totalMinutes = Math.floor(ms / 60000);
  if (totalMinutes < 60) {
    return `${totalMinutes} min ago`;
  }

  const hours = Math.floor(totalMinutes / 60);
  const minutes = totalMinutes % 60;
  return minutes > 0 ? `${hours}h ${minutes}m ago` : `${hours}h ago`;
}

function formatDuration(ms) {
  if (!ms || ms <= 0) {
    return "0m";
  }

  const totalMinutes = Math.floor(ms / 60000);
  const days = Math.floor(totalMinutes / 1440);
  const hours = Math.floor((totalMinutes % 1440) / 60);
  const minutes = totalMinutes % 60;

  if (days > 0) {
    return `${days}d ${hours}h`;
  }

  if (hours > 0) {
    return `${hours}h ${minutes}m`;
  }

  return `${minutes}m`;
}

function HealthBadge({ reading }) {
  if (!reading) {
    return <span className="badge badge-muted">Waiting for data</span>;
  }

  const score = [reading.pm2p5, reading.pm10, reading.coPpb, reading.no2Ppb].filter(
    (value) => value !== null && value !== undefined,
  ).length;

  if (score >= 4) {
    return <span className="badge badge-good">Sensors healthy</span>;
  }

  if (score >= 2) {
    return <span className="badge badge-warn">Partial sensor data</span>;
  }

  return <span className="badge badge-danger">Low confidence data</span>;
}

function MetricCard({ label, value, unit, accent }) {
  return (
    <article className={`metric-card ${accent}`}>
      <p className="metric-label">{label}</p>
      <p className="metric-value">{formatValue(value, unit)}</p>
    </article>
  );
}

function clampPercent(value, max) {
  if (value === null || value === undefined || Number.isNaN(value) || max <= 0) {
    return 0;
  }

  return Math.max(0, Math.min((Number(value) / max) * 100, 100));
}

function GaugeCard({ title, subtitle, value, unit, max, colorClass }) {
  const percent = clampPercent(value, max);

  return (
    <article className="gauge-card">
      <div className="gauge-header">
        <div>
          <p className="panel-kicker">{subtitle}</p>
          <h3>{title}</h3>
        </div>
      </div>
      <div className={`gauge-shell ${colorClass}`} style={{ "--gauge-value": `${percent}%` }}>
        <div className="gauge-center">
          <p className="gauge-number">{formatCompactValue(value)}</p>
          <p className="gauge-unit">{unit}</p>
        </div>
      </div>
    </article>
  );
}

function average(values) {
  const valid = values.filter((value) => value !== null && value !== undefined && !Number.isNaN(value));
  if (valid.length === 0) {
    return null;
  }

  return valid.reduce((sum, value) => sum + Number(value), 0) / valid.length;
}

function averageAbsoluteGap(values) {
  if (values.length < 2) {
    return null;
  }

  let total = 0;
  for (let index = 1; index < values.length; index += 1) {
    total += Math.abs(values[index] - values[index - 1]);
  }

  return total / (values.length - 1);
}

function computeHealthStatus(readings, config) {
  if (!config.healthAverageWindowMs || !config.healthBreakpoints) {
    return null;
  }

  const validSamples = (readings ?? []).filter(
    (reading) => reading[config.key] !== null && reading[config.key] !== undefined,
  );

  if (validSamples.length === 0) {
    return {
      label: "No data",
      tone: "warming",
      average: null,
      windowCoveredMs: 0,
      requiredWindowMs: config.healthAverageWindowMs,
      detail: "No readings for health averaging yet",
    };
  }

  const latestTimestamp =
    validSamples[validSamples.length - 1].capturedAt ??
    validSamples[validSamples.length - 1]._creationTime;
  const windowSamples = validSamples.filter((reading) => {
    const timestamp = reading.capturedAt ?? reading._creationTime;
    return latestTimestamp - timestamp <= config.healthAverageWindowMs;
  });

  const firstTimestamp = windowSamples[0].capturedAt ?? windowSamples[0]._creationTime;
  const windowCoveredMs = latestTimestamp - firstTimestamp;
  const values = windowSamples.map((reading) => Number(reading[config.key]));
  const avg = average(values);

  if (windowCoveredMs < Math.min(config.healthAverageWindowMs, 30 * 60 * 1000)) {
    return {
      label: "Averaging",
      tone: "warming",
      average: avg,
      windowCoveredMs,
      requiredWindowMs: config.healthAverageWindowMs,
      detail: `Collecting more data for ${Math.round(config.healthAverageWindowMs / 3600000)}h average`,
    };
  }

  const category =
    config.healthBreakpoints.find((breakpoint) => avg <= breakpoint.max) ??
    config.healthBreakpoints[config.healthBreakpoints.length - 1];

  return {
    label: category.label,
    tone: category.tone,
    average: avg,
    windowCoveredMs,
    requiredWindowMs: config.healthAverageWindowMs,
    detail: `Average used: ${formatCompactValue(avg)} ${config.unit} over ${formatDuration(windowCoveredMs)}`,
  };
}

function computeStability(readings, config) {
  if (config.mode === "dgs2") {
    const validSamples = (readings ?? []).filter(
      (reading) => reading[config.key] !== null && reading[config.key] !== undefined,
    );
    const latestTimestamp = validSamples.length
      ? validSamples[validSamples.length - 1].capturedAt ?? validSamples[validSamples.length - 1]._creationTime
      : null;

    if (!latestTimestamp) {
      const health = computeHealthStatus(readings, config);
      return {
        ...config,
        stable: false,
        level: "warming",
        reason: "No readings yet",
        detail: "Waiting for the first packet",
        health,
        spread: null,
        since: null,
        latest: null,
      };
    }

    const samples = validSamples.filter((reading) => {
      const timestamp = reading.capturedAt ?? reading._creationTime;
      return latestTimestamp - timestamp <= config.targetWindowMs;
    });

    const windowDuration =
      (samples.at(-1)?.capturedAt ?? samples.at(-1)?._creationTime ?? latestTimestamp) -
      (samples[0]?.capturedAt ?? samples[0]?._creationTime ?? latestTimestamp);

    if (samples.length < config.minPoints || windowDuration < config.minWindowMs) {
      const reasons = [];
      if (samples.length < config.minPoints) {
        reasons.push(`${samples.length}/${config.minPoints} valid readings`);
      }
      if (windowDuration < config.minWindowMs) {
        reasons.push(`${Math.round(windowDuration / 60000)} of 30 min covered`);
      }

      const health = computeHealthStatus(readings, config);
      return {
        ...config,
        stable: false,
        level: "warming",
        reason: "Not enough 30-60 minute history yet",
        detail: reasons.join(", "),
        health,
        spread: null,
        since: samples[0]?.capturedAt ?? samples[0]?._creationTime ?? null,
        latest: samples.at(-1)?.[config.key] ?? null,
      };
    }

    const values = samples.map((reading) => Number(reading[config.key]));
    const avg = average(values);
    const spread = Math.max(...values) - Math.min(...values);
    const drift = Math.abs(values[values.length - 1] - values[0]);
    const avgGap = averageAbsoluteGap(values) ?? 0;
    const maxMetric = Math.max(spread, drift, avgGap);
    const since = samples[0].capturedAt ?? samples[0]._creationTime;
    const health = computeHealthStatus(readings, config);

    let blockingMetric = "spread";
    let blockingValue = spread;

    if (drift >= blockingValue) {
      blockingMetric = "drift";
      blockingValue = drift;
    }

    if (avgGap >= blockingValue) {
      blockingMetric = "avg gap";
      blockingValue = avgGap;
    }

    if (maxMetric <= config.goodThreshold) {
      return {
        ...config,
        stable: true,
        level: "good",
        reason: `Good clean-air stability over ${Math.round(windowDuration / 60000)} min`,
        detail: `avg ${avg.toFixed(1)}, avg gap ${avgGap.toFixed(1)}, drift ${drift.toFixed(1)}, spread ${spread.toFixed(1)} ${config.unit}`,
        health,
        spread,
        since,
        latest: values.at(-1) ?? null,
      };
    }

    if (maxMetric <= config.acceptableThreshold) {
      return {
        ...config,
        stable: true,
        level: "acceptable",
        reason: `Acceptable clean-air stability over ${Math.round(windowDuration / 60000)} min`,
        detail: `avg ${avg.toFixed(1)}, avg gap ${avgGap.toFixed(1)}, drift ${drift.toFixed(1)}, spread ${spread.toFixed(1)} ${config.unit}`,
        health,
        spread,
        since,
        latest: values.at(-1) ?? null,
      };
    }

    return {
      ...config,
      stable: false,
      level: "warming",
      reason: `Still stabilizing over ${Math.round(windowDuration / 60000)} min`,
      detail: `${blockingMetric} ${blockingValue.toFixed(1)} ${config.unit} is above acceptable ${config.acceptableThreshold} ${config.unit}. avg ${avg.toFixed(1)}, avg gap ${avgGap.toFixed(1)}, drift ${drift.toFixed(1)}, spread ${spread.toFixed(1)}`,
      health,
      spread,
      since,
      latest: values.at(-1) ?? null,
    };
  }

  const samples = (readings ?? [])
    .filter((reading) => reading[config.key] !== null && reading[config.key] !== undefined)
    .slice(-config.minPoints);
  const health = computeHealthStatus(readings, config);

  if (samples.length < config.minPoints) {
    return {
      ...config,
      stable: false,
      level: "warming",
      reason: "Not enough readings yet",
      detail: `${samples.length}/${config.minPoints} valid packets`,
      health,
      spread: null,
      since: null,
      latest: samples.at(-1)?.[config.key] ?? null,
    };
  }

  const values = samples.map((reading) => Number(reading[config.key]));
  const min = Math.min(...values);
  const max = Math.max(...values);
  const spread = max - min;
  const since = samples[0].capturedAt ?? samples[0]._creationTime;

  if (spread <= config.tolerance) {
    return {
      ...config,
      stable: true,
      level: "good",
      reason: `Stable across last ${samples.length} packets`,
      detail: `Spread ${spread.toFixed(1)} ${config.unit}`,
      health,
      spread,
      since,
      latest: values.at(-1) ?? null,
    };
  }

  return {
    ...config,
    stable: false,
    level: "warming",
    reason: `Still moving across last ${samples.length} packets`,
    detail: `Spread ${spread.toFixed(1)} ${config.unit} is above tolerance ${config.tolerance} ${config.unit}`,
    health,
    spread,
    since,
    latest: values.at(-1) ?? null,
  };
}

function buildGasChartPoints(readings, range) {
  if (!readings || readings.length === 0) {
    return [];
  }

  const latestTimestamp = Math.max(
    ...readings.map((reading) => reading.capturedAt ?? reading._creationTime),
  );
  const latestDate = new Date(latestTimestamp);
  const startOfLatestDay = new Date(
    latestDate.getFullYear(),
    latestDate.getMonth(),
    latestDate.getDate(),
  ).getTime();
  const windowStart = range.key === "today"
    ? startOfLatestDay
    : latestTimestamp - range.windowMs;
  const buckets = new Map();

  readings.forEach((reading) => {
    const timestamp = reading.capturedAt ?? reading._creationTime;
    if (timestamp < windowStart) {
      return;
    }

    const bucket = Math.floor((timestamp - windowStart) / range.bucketMs);
    const entry = buckets.get(bucket) ?? {
      timestamp: windowStart + bucket * range.bucketMs,
      no2Ppb: [],
      so2Ppb: [],
      coPpb: [],
      o3Ppb: [],
    };

    gasSeries.forEach((series) => {
      entry[series.key].push(reading[series.key]);
    });

    buckets.set(bucket, entry);
  });

  return Array.from(buckets.values())
    .sort((a, b) => a.timestamp - b.timestamp)
    .map((bucket) => ({
      timestamp: bucket.timestamp,
      no2Ppb: average(bucket.no2Ppb),
      so2Ppb: average(bucket.so2Ppb),
      coPpb: average(bucket.coPpb),
      o3Ppb: average(bucket.o3Ppb),
    }));
}

function buildLinePath(points, key, chartWidth, chartHeight, minValue, maxValue) {
  const valid = points
    .map((point, index) => ({
      x: points.length === 1 ? chartWidth / 2 : (index / (points.length - 1)) * chartWidth,
      y:
        chartHeight -
        ((point[key] - minValue) / Math.max(maxValue - minValue, 1)) * chartHeight,
      value: point[key],
    }))
    .filter((point) => point.value !== null && point.value !== undefined && !Number.isNaN(point.value));

  if (valid.length === 0) {
    return "";
  }

  return valid
    .map((point, index) => `${index === 0 ? "M" : "L"} ${point.x.toFixed(2)} ${point.y.toFixed(2)}`)
    .join(" ");
}

function GasTrendChart({ readings, rangeKey, onRangeChange, visibleSensors, onToggleSensor }) {
  const selectedRange = rangeOptions.find((option) => option.key === rangeKey) ?? rangeOptions[1];
  const points = buildGasChartPoints(readings ?? [], selectedRange);
  const activeSeries = gasSeries.filter((series) => visibleSensors.includes(series.key));
  const chartWidth = 640;
  const chartHeight = 260;
  const allValues = points.flatMap((point) =>
    activeSeries.map((series) => point[series.key]).filter(
      (value) => value !== null && value !== undefined && !Number.isNaN(value),
    ),
  );
  const maxValue = allValues.length > 0 ? Math.max(...allValues) : 100;
  const yMax = Math.max(10, Math.ceil(maxValue / 10) * 10);

  return (
    <article className="chart-panel">
      <div className="panel-header">
        <div>
          <p className="panel-kicker">Sensor trends</p>
          <h2>NO2, SO2, CO and O3</h2>
        </div>
        <div className="range-tabs">
          {rangeOptions.map((option) => (
            <button
              key={option.key}
              type="button"
              onClick={() => onRangeChange(option.key)}
              className={option.key === rangeKey ? "range-tab active" : "range-tab"}
            >
              {option.label}
            </button>
          ))}
        </div>
      </div>

      <div className="chart-legend">
        {gasSeries.map((series) => (
          <button
            key={series.key}
            type="button"
            onClick={() => onToggleSensor(series.key)}
            className={visibleSensors.includes(series.key) ? "sensor-chip active" : "sensor-chip"}
          >
            <i style={{ backgroundColor: series.color }} />
            {series.label}
          </button>
        ))}
      </div>

      <div className="chart-wrap">
        <div className="chart-y-axis">
          {[yMax, yMax * 0.66, yMax * 0.33, 0].map((tick) => (
            <span key={tick}>{Math.round(tick)}</span>
          ))}
        </div>
        <svg viewBox={`0 0 ${chartWidth} ${chartHeight}`} className="trend-chart" preserveAspectRatio="none">
          {[0.25, 0.5, 0.75].map((ratio) => (
            <line
              key={ratio}
              x1="0"
              y1={chartHeight * ratio}
              x2={chartWidth}
              y2={chartHeight * ratio}
              className="grid-line"
            />
          ))}
          {activeSeries.map((series) => {
            const path = buildLinePath(points, series.key, chartWidth, chartHeight, 0, yMax);
            return path ? (
              <path
                key={series.key}
                d={path}
                fill="none"
                stroke={series.color}
                strokeWidth="3"
                strokeLinecap="round"
                strokeLinejoin="round"
              />
            ) : null;
          })}
          {activeSeries.flatMap((series) =>
            points.map((point, index) => {
              const value = point[series.key];
              if (value === null || value === undefined || Number.isNaN(value)) {
                return null;
              }

              const x = points.length === 1 ? chartWidth / 2 : (index / (points.length - 1)) * chartWidth;
              const y = chartHeight - (value / Math.max(yMax, 1)) * chartHeight;

              return (
                <circle
                  key={`${series.key}-${point.timestamp}`}
                  cx={x}
                  cy={y}
                  r="3.5"
                  fill={series.color}
                />
              );
            }),
          )}
        </svg>
      </div>

      <div className="chart-x-axis">
        {points.length > 0 && activeSeries.length > 0 ? (
          points.map((point) => <span key={point.timestamp}>{formatAxisLabel(point.timestamp, selectedRange.key)}</span>)
        ) : (
          <span>{activeSeries.length === 0 ? "Select at least one sensor" : "No data yet"}</span>
        )}
      </div>
    </article>
  );
}

function RangeTable({ readings, rangeKey }) {
  const selectedRange = rangeOptions.find((option) => option.key === rangeKey) ?? rangeOptions[1];
  const latestTimestamp = readings?.length
    ? Math.max(...readings.map((reading) => reading.capturedAt ?? reading._creationTime))
    : 0;
  const latestDate = new Date(latestTimestamp);
  const startOfLatestDay = new Date(
    latestDate.getFullYear(),
    latestDate.getMonth(),
    latestDate.getDate(),
  ).getTime();
  const filtered = (readings ?? []).filter((reading) => {
    const timestamp = reading.capturedAt ?? reading._creationTime;
    if (selectedRange.key === "today") {
      return timestamp >= startOfLatestDay && timestamp <= latestTimestamp;
    }

    return latestTimestamp - timestamp <= selectedRange.windowMs;
  });

  return (
    <div className="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>NO2</th>
            <th>SO2</th>
            <th>CO</th>
            <th>O3</th>
            <th>Temp</th>
            <th>Humidity</th>
          </tr>
        </thead>
        <tbody>
          {filtered.map((reading) => (
            <tr key={reading._id}>
              <td>{formatDate(reading.capturedAt ?? reading._creationTime)}</td>
              <td>{formatValue(reading.no2Ppb, "ppb")}</td>
              <td>{formatValue(reading.so2Ppb, "ppb")}</td>
              <td>{formatValue(reading.coPpb, "ppb")}</td>
              <td>{formatValue(reading.o3Ppb, "ppb")}</td>
              <td>{formatValue(reading.temperatureC, "C")}</td>
              <td>{formatValue(reading.humidityRh, "%")}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function StabilityPanel({ readings }) {
  const summaries = stabilityConfig.map((config) => computeStability(readings, config));

  return (
    <section className="panel">
      <div className="panel-header">
        <div>
          <p className="panel-kicker">Stabilization check</p>
          <h2>Sensor settling summary</h2>
        </div>
      </div>

      <div className="stability-grid">
        {summaries.map((summary) => (
          <article
            key={summary.key}
            className={`stability-card ${
              summary.level === "good"
                ? "stable"
                : summary.level === "acceptable"
                  ? "acceptable"
                  : "warming"
            }`}
          >
            <div className="stability-top">
              <h3>{summary.label}</h3>
              <span
                className={`status-pill ${
                  summary.level === "good"
                    ? "stable"
                    : summary.level === "acceptable"
                      ? "acceptable"
                      : "warming"
                }`}
              >
                {summary.level === "good"
                  ? "Good"
                  : summary.level === "acceptable"
                    ? "Acceptable"
                    : "Stabilizing"}
              </span>
            </div>
            <p className="stability-reading">
              {formatCompactValue(summary.latest)} {summary.unit}
            </p>
            <p className="stability-text">{summary.reason}</p>
            <p className="stability-text secondary">{summary.detail}</p>
            {summary.health ? (
              <div className={`air-status air-${summary.health.tone}`}>
                <p className="air-status-label">
                  {summary.stable ? "Air range" : "Air range (wait for stability)"}
                </p>
                <p className="air-status-value">{summary.health.label}</p>
                <p className="air-status-detail">
                  {summary.health.detail}
                </p>
              </div>
            ) : null}
            <p className="stability-time">
              {summary.since ? `Window start: ${formatDate(summary.since)}` : "Waiting for more samples"}
            </p>
          </article>
        ))}
      </div>
    </section>
  );
}

function WarmupPanel({ readings, now, resetAt, onReset, isResetting }) {
  const firstReadingTimestamp = readings?.length
    ? readings[0].capturedAt ?? readings[0]._creationTime
    : null;
  const baselineTimestamp = resetAt
    ? firstReadingTimestamp
      ? Math.max(firstReadingTimestamp, resetAt)
      : resetAt
    : firstReadingTimestamp;
  const observedRuntimeMs = baselineTimestamp ? Math.max(0, now - baselineTimestamp) : 0;
  const remainingMs = Math.max(0, DGS2_WARMUP_MS - observedRuntimeMs);
  const warmupComplete = baselineTimestamp ? observedRuntimeMs >= DGS2_WARMUP_MS : false;
  const percent = Math.min((observedRuntimeMs / DGS2_WARMUP_MS) * 100, 100);

  return (
    <section className="panel warmup-panel">
      <div className="panel-header">
        <div>
          <p className="panel-kicker">DGS2 warm-up</p>
          <h2>Observed 24-hour readiness</h2>
        </div>
        <div className="warmup-actions">
          <span className={warmupComplete ? "status-pill stable" : "status-pill warming"}>
            {warmupComplete ? "Ready" : "Warming up"}
          </span>
          <button
            type="button"
            className="range-tab"
            onClick={onReset}
            disabled={isResetting}
          >
            {isResetting ? "Resetting..." : "Reset 24h"}
          </button>
        </div>
      </div>

      <div className="warmup-grid">
        <div className="warmup-progress">
          <div className="warmup-track">
            <div className="warmup-fill" style={{ width: `${percent}%` }} />
          </div>
          <p className="warmup-percent">{Math.round(percent)}%</p>
        </div>

        <div className="warmup-stats">
          <article>
            <p className="panel-kicker">Observed runtime</p>
            <h3>{formatDuration(observedRuntimeMs)}</h3>
          </article>
          <article>
            <p className="panel-kicker">Remaining</p>
            <h3>{warmupComplete ? "0m" : formatDuration(remainingMs)}</h3>
          </article>
          <article>
            <p className="panel-kicker">First logged packet</p>
            <h3>{baselineTimestamp ? formatDate(baselineTimestamp) : "--"}</h3>
          </article>
        </div>
      </div>

      <p className="warmup-note">
        This timer is based on the later of the first stored reading or your last manual reset. Use
        `Reset 24h` whenever the DGS2 is powered off so the observed stabilization window starts over.
      </p>
    </section>
  );
}

export default function App() {
  const latest = useQuery(api.readings.getLatest);
  const readings = useQuery(api.readings.listRecent, { limit: 500 });
  const packetGapSummary = useQuery(api.readings.getPacketGapSummary);
  const warmupReset = useQuery(api.readings.getWarmupReset);
  const resetWarmupTimer = useMutation(api.readings.resetWarmupTimer);
  const trend = readings ? [...readings].reverse() : [];
  const [selectedRange, setSelectedRange] = useState("day");
  const [visibleSensors, setVisibleSensors] = useState(gasSeries.map((series) => series.key));
  const [now, setNow] = useState(Date.now());
  const [isResettingWarmup, setIsResettingWarmup] = useState(false);
  const hasNotifiedRef = useRef(false);
  const latestTimestamp = latest ? latest.capturedAt ?? latest._creationTime : null;
  const staleForMs = latestTimestamp ? now - latestTimestamp : 0;
  const isStale = latestTimestamp ? staleForMs >= STALE_PACKET_MS : false;

  useEffect(() => {
    const timer = window.setInterval(() => {
      setNow(Date.now());
    }, 60000);

    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!("Notification" in window)) {
      return;
    }

    if (Notification.permission === "default") {
      Notification.requestPermission();
    }
  }, []);

  useEffect(() => {
    if (!latestTimestamp) {
      return;
    }

    if (!isStale) {
      hasNotifiedRef.current = false;
      return;
    }

    if (hasNotifiedRef.current || !("Notification" in window)) {
      return;
    }

    if (Notification.permission === "granted") {
      new Notification("Air quality node offline", {
        body: `No packet has been received for ${formatAge(staleForMs)}.`,
      });
      hasNotifiedRef.current = true;
    }
  }, [isStale, latestTimestamp, staleForMs]);

  const latestAgeText = useMemo(() => {
    if (!latestTimestamp) {
      return "No packet received yet";
    }

    return formatAge(staleForMs);
  }, [latestTimestamp, staleForMs]);

  async function handleResetWarmup() {
    try {
      setIsResettingWarmup(true);
      await resetWarmupTimer({});
      setNow(Date.now());
    } finally {
      setIsResettingWarmup(false);
    }
  }

  function handleToggleSensor(sensorKey) {
    setVisibleSensors((current) => {
      if (current.includes(sensorKey)) {
        return current.length === 1 ? current : current.filter((key) => key !== sensorKey);
      }

      return [...current, sensorKey];
    });
  }

  return (
    <main className="app-shell">
      {isStale ? (
        <section className="alert-banner">
          <strong>Packet alert:</strong> this device has not sent data for {latestAgeText}. Check power,
          LoRa link, or Wi-Fi forwarding.
        </section>
      ) : null}

      <section className="hero">
        <div>
          <p className="eyebrow">Air Quality Monitoring</p>
          <h1>Live Environmental Dashboard</h1>
          <p className="hero-copy">
            Real-time values from your LoRa sensor node, synced through ESP32 and
            Convex.
          </p>
          <p className="hero-copy note">
            DGS2 stability uses your clean-air prototype rule: 30-60 minute window,
            average gap, drift, and spread against gas-specific thresholds.
          </p>
        </div>
        <div className="hero-panel">
          <p className="hero-label">Latest packet</p>
          <p className="hero-node">{latest?.nodeId ?? "No node yet"}</p>
          <p className="hero-time">{formatDate(latest?.capturedAt ?? latest?._creationTime)}</p>
          <p className="hero-age">Last update: {latestAgeText}</p>
          <HealthBadge reading={latest} />
        </div>
      </section>

      <section className="gauge-grid">
        <GaugeCard
          title="Temperature"
          subtitle="Comfort dial"
          value={latest?.temperatureC}
          unit="C"
          max={50}
          colorClass="gauge-coral"
        />
        <GaugeCard
          title="Humidity"
          subtitle="Moisture dial"
          value={latest?.humidityRh}
          unit="%"
          max={100}
          colorClass="gauge-sky"
        />
      </section>

      <section className="metrics-grid">
        {overviewCards.map((metric) => (
          <MetricCard
            key={metric.key}
            label={metric.label}
            unit={metric.unit}
            value={latest?.[metric.key]}
            accent={metric.accent}
          />
        ))}
      </section>

      <WarmupPanel
        readings={trend}
        now={now}
        resetAt={warmupReset?.resetAt ?? null}
        onReset={handleResetWarmup}
        isResetting={isResettingWarmup}
      />

      <StabilityPanel readings={trend} />

      <section className="trend-section">
        <GasTrendChart
          readings={trend}
          rangeKey={selectedRange}
          onRangeChange={setSelectedRange}
          visibleSensors={visibleSensors}
          onToggleSensor={handleToggleSensor}
        />
      </section>

      <article className="panel packet-details-panel">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">Packet details</p>
            <h2>Current payload</h2>
          </div>
        </div>

        <div className="packet-gap-summary">
          <article>
            <p className="panel-kicker">Missing packets</p>
            <h3>{packetGapSummary?.totalMissingPackets ?? "--"}</h3>
          </article>
          <article>
            <p className="panel-kicker">Gap events</p>
            <h3>{packetGapSummary?.gapEvents ?? "--"}</h3>
          </article>
          <article>
            <p className="panel-kicker">Longest gap</p>
            <h3>{packetGapSummary?.longestGapPackets ?? "--"}</h3>
          </article>
          <article>
            <p className="panel-kicker">Sequence resets</p>
            <h3>{packetGapSummary?.resetsDetected ?? "--"}</h3>
          </article>
        </div>

        <p className="packet-gap-note">
          Missing packets are counted when the current packet sequence jumps by more than 1. If
          sequence is unavailable, the dashboard falls back to 1-minute timing gaps.
        </p>

        {packetGapSummary?.latestGap ? (
          <div className="latest-gap-card">
            <p className="panel-kicker">Latest detected gap</p>
            <p className="latest-gap-main">
              {packetGapSummary.latestGap.missingPackets} missing packet
              {packetGapSummary.latestGap.missingPackets === 1 ? "" : "s"}
            </p>
            <p className="latest-gap-sub">
              Between {formatDate(packetGapSummary.latestGap.fromTimestamp)} and{" "}
              {formatDate(packetGapSummary.latestGap.toTimestamp)}
            </p>
            <p className="latest-gap-sub">
              {packetGapSummary.latestGap.fromSequence !== null &&
              packetGapSummary.latestGap.toSequence !== null
                ? `Sequence ${packetGapSummary.latestGap.fromSequence} to ${packetGapSummary.latestGap.toSequence}`
                : "Detected from packet timing"}
            </p>
          </div>
        ) : (
          <p className="packet-gap-note">
            {packetGapSummary
              ? "No missing packet gaps detected in the full stored history."
              : "Loading full packet history summary..."}
          </p>
        )}

        <dl className="details-list">
          <div>
            <dt>Sequence</dt>
            <dd>{latest?.sequence ?? "--"}</dd>
          </div>
          <div>
            <dt>Status</dt>
            <dd>{latest?.status ?? "--"}</dd>
          </div>
          <div>
            <dt>Validity mask</dt>
            <dd>{latest?.validityMask ?? "--"}</dd>
          </div>
          <div>
            <dt>CRC</dt>
            <dd>{latest?.crc ?? "--"}</dd>
          </div>
          <div>
            <dt>CRC check</dt>
            <dd>{latest?.crcOk === undefined ? "--" : latest.crcOk ? "PASS" : "FAIL"}</dd>
          </div>
          <div>
            <dt>Raw payload</dt>
            <dd className="payload">{latest?.rawPayload ?? "--"}</dd>
          </div>
          <div>
            <dt>RSSI</dt>
            <dd>{latest?.rssi ?? "--"}</dd>
          </div>
          <div>
            <dt>SNR</dt>
            <dd>{latest?.snr ?? "--"}</dd>
          </div>
        </dl>
      </article>

      <section className="panel">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">Historical log</p>
            <h2>Gas and climate table</h2>
          </div>
          <div className="range-tabs">
            {rangeOptions.map((option) => (
              <button
                key={option.key}
                type="button"
                onClick={() => setSelectedRange(option.key)}
                className={option.key === selectedRange ? "range-tab active" : "range-tab"}
              >
                {option.label}
              </button>
            ))}
          </div>
        </div>

        <RangeTable readings={trend} rangeKey={selectedRange} />
      </section>
    </main>
  );
}
