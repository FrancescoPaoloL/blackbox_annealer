#!/bin/bash
set -e

VICTIM_PORT=8081
EMBED_PORT=8082
THREADS_VICTIM=4
THREADS_EMBED=2
SEED_FILE=/app/seeds/seed_01.txt

echo "[entrypoint] starting victim server (Qwen) on :$VICTIM_PORT"
/app/bin/llama-server \
    -m /app/models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
    --host 127.0.0.1 --port $VICTIM_PORT \
    -c 1024 -n 120 \
    --threads $THREADS_VICTIM \
    --log-disable &

echo "[entrypoint] starting embedding server (MiniLM) on :$EMBED_PORT"
/app/bin/llama-server \
    -m /app/models/all-MiniLM-L6-v2-Q8_0.gguf \
    --host 127.0.0.1 --port $EMBED_PORT \
    --embedding \
    --threads $THREADS_EMBED \
    --log-disable &

echo "[entrypoint] waiting for servers..."
for PORT in $VICTIM_PORT $EMBED_PORT; do
    until curl -sf http://127.0.0.1:$PORT/health > /dev/null 2>&1; do
        sleep 1
    done
    echo "[entrypoint] server :$PORT ready"
done

mkdir -p /app/results

echo "[entrypoint] starting annealer"
/app/annealer/annealer $SEED_FILE

echo "[entrypoint] done"
cat /app/results/best.txt

