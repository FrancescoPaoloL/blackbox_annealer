FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    libgomp1 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN mkdir -p bin lib models annealer mutator guardian seeds results bench

COPY llama-bin/bin/llama-server  ./bin/
COPY llama-bin/lib/              ./lib/

ENV LD_LIBRARY_PATH=/app/lib

COPY models/all-MiniLM-L6-v2-Q8_0.gguf  ./models/

COPY annealer/annealer   ./annealer/
COPY mutator/mutator.py  ./mutator/
COPY guardian/guardian.py ./guardian/
COPY bench/bench.py      ./bench/
COPY seeds/seed_01.txt   ./seeds/

COPY docker/entrypoint.sh ./entrypoint.sh
RUN chmod +x ./entrypoint.sh ./bin/llama-server ./annealer/annealer

ENTRYPOINT ["./entrypoint.sh"]

