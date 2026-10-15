#!/bin/sh
set -e

TCP_PORT="${TCP_PORT:-54897}"
INPUT_FILE="${INPUT_FILE:-input.txt}"
DEFAULT_MAX_PLAYERS="${DEFAULT_MAX_PLAYERS:-2}"
HTTP_PORT="${PORT:-8080}"

echo "[entrypoint] Starting TypeRacer C Engine daemon on port $TCP_PORT..."
/app/server --daemon "$TCP_PORT" "$INPUT_FILE" "$DEFAULT_MAX_PLAYERS" &
ENGINE_PID=$!

echo "[entrypoint] Starting TypeRacer Node.js Gateway on port $HTTP_PORT..."
PORT="$HTTP_PORT" C_SERVER_PORT="$TCP_PORT" node /app/gateway/gateway.js &
GATEWAY_PID=$!

shutdown() {
    EXIT_STATUS="${1:-0}"
    echo "[entrypoint] Shutting down services (exit code: $EXIT_STATUS)..."
    kill -TERM "$ENGINE_PID" 2>/dev/null || true
    kill -TERM "$GATEWAY_PID" 2>/dev/null || true
    wait "$ENGINE_PID" 2>/dev/null || true
    wait "$GATEWAY_PID" 2>/dev/null || true
    echo "[entrypoint] Shutdown complete."
    exit "$EXIT_STATUS"
}

trap 'shutdown 0' INT TERM

# Monitor child processes until either exits
while kill -0 "$ENGINE_PID" 2>/dev/null && kill -0 "$GATEWAY_PID" 2>/dev/null; do
    sleep 1
done

echo "[entrypoint] One of the background services terminated. Stopping container."
shutdown 1
