/**
 * TypeRacer Gateway & Connection Ruleset Configuration
 *
 * Centralizes all networking, timeout, and keepalive configurations.
 * Values can be tuned dynamically via environment variables without rebuilding images.
 */

function parsePositiveInt(val, defaultVal) {
  if (val === undefined || val === null || val === '') return defaultVal;
  const num = parseInt(val, 10);
  return Number.isFinite(num) && num >= 0 ? num : defaultVal;
}

const config = {
  // Service binding & engine transport
  HTTP_PORT: parsePositiveInt(process.env.PORT, 8080),
  C_SERVER_HOST: process.env.C_SERVER_HOST || '127.0.0.1',
  C_SERVER_PORT: parsePositiveInt(process.env.C_SERVER_PORT, 54897),

  // Connection ruleset & timeouts
  // Maximum time to wait in a multiplayer lobby before timing out (default 10 minutes)
  LOBBY_TIMEOUT_MS: parsePositiveInt(
    process.env.LOBBY_TIMEOUT_MS || process.env.LOBBY_WAIT_TIMEOUT_MS,
    600000
  ),

  // Single-shot TCP command timeout for REST API calls (HEALTH, CREATE, SUMMARY)
  TCP_COMMAND_TIMEOUT_MS: parsePositiveInt(
    process.env.TCP_COMMAND_TIMEOUT_MS,
    2500
  ),

  // TCP keepalive probe interval on backend engine sockets (prevents NAT/proxy drops)
  TCP_KEEPALIVE_MS: parsePositiveInt(
    process.env.TCP_KEEPALIVE_MS,
    15000
  ),

  // WebSocket ping heartbeat interval to prevent intermediate reverse proxies (NGINX, Cloudflare, ALB) from closing idle connections
  WS_HEARTBEAT_INTERVAL_MS: parsePositiveInt(
    process.env.WS_HEARTBEAT_INTERVAL_MS,
    30000
  ),

  // Maximum players allowed in a lobby
  MAX_LOBBY_CAPACITY: parsePositiveInt(
    process.env.MAX_LOBBY_CAPACITY || process.env.DEFAULT_MAX_PLAYERS,
    4
  ),

  // Post-finish grace period countdown (seconds) after first player crosses the finish line
  FINISH_GRACE_TIMEOUT_SEC: parsePositiveInt(
    process.env.FINISH_GRACE_TIMEOUT_SEC,
    20
  ),

  // Maximum race duration ceiling (seconds) before forcing DNF on unfinished racers
  RACE_MAX_DURATION_SEC: parsePositiveInt(
    process.env.RACE_MAX_DURATION_SEC,
    60
  )
};

module.exports = config;
