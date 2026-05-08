#!/usr/bin/env bash
set -eu

HOST="${PISUGAR_WEB_HOOK_HOST:-127.0.0.1}"
PORT="${WHISPLAY_WEB_PORT:-17880}"

exec /usr/bin/curl \
  --silent \
  --show-error \
  --max-time 20 \
  --request POST \
  "http://${HOST}:${PORT}/api/vision/capture" \
  >/dev/null
