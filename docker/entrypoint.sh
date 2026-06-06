#!/bin/bash
set -e

EMBED_PORT=8082
SEED_FILE=/app/seeds/seed_01.txt

echo "[entrypoint] starting embedding server (MiniLM) on :$EMBED_PORT"
/app/bin/llama-server \
    -m /app/models/all-MiniLM-L6-v2-Q8_0.gguf \
    --host 127.0.0.1 --port $EMBED_PORT \
    --embedding \
    --threads 2 \
    --log-disable &

echo "[entrypoint] waiting for server..."
until curl -sf http://127.0.0.1:$EMBED_PORT/health > /dev/null 2>&1; do
    sleep 1
done
echo "[entrypoint] server :$EMBED_PORT ready"

mkdir -p /app/results

echo "[entrypoint] starting annealer"
/app/annealer/annealer $SEED_FILE

echo "[entrypoint] done"
cat /app/results/best.txt

