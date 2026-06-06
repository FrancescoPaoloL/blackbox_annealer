# -----------------------------------------------------------------------
# blackbox_annealer: single container
# Three processes: llama-server (victim), llama-server (embedding), annealer
# No exposed ports: runs to completion, logs via az container logs
# -----------------------------------------------------------------------

FROM debian:bookworm-slim

# -----------------------------------------------------------------------
# System deps
# -----------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    libcurl4 \
    libgomp1 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# -----------------------------------------------------------------------
# Directory layout
# -----------------------------------------------------------------------
WORKDIR /app

RUN mkdir -p bin lib models annealer mutator seeds results

# -----------------------------------------------------------------------
# llama-server binary + shared libs (copied by build.sh)
# -----------------------------------------------------------------------
COPY llama-bin/bin/llama-server  ./bin/
COPY llama-bin/lib/              ./lib/

ENV LD_LIBRARY_PATH=/app/lib

# -----------------------------------------------------------------------
# Models
# -----------------------------------------------------------------------
COPY models/qwen2.5-0.5b-instruct-q4_k_m.gguf  ./models/
COPY models/all-MiniLM-L6-v2-Q8_0.gguf          ./models/

# -----------------------------------------------------------------------
# Annealer binary
# -----------------------------------------------------------------------
COPY annealer/annealer  ./annealer/

# -----------------------------------------------------------------------
# Mutator + seed
# -----------------------------------------------------------------------
COPY mutator/mutator.py  ./mutator/
COPY seeds/seed_01.txt   ./seeds/

# -----------------------------------------------------------------------
# Entrypoint
# -----------------------------------------------------------------------
COPY docker/entrypoint.sh ./entrypoint.sh
RUN chmod +x ./entrypoint.sh ./bin/llama-server ./annealer/annealer

ENTRYPOINT ["./entrypoint.sh"]

