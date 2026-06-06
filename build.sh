#!/bin/bash
# -----------------------------------------------------------------------
# build.sh: build everything for Azure (AMD EPYC) and package
# Run this locally before docker build
# -----------------------------------------------------------------------
set -e

LLAMA_DIR="../llama.cpp"
ANNEALER_DIR="./annealer"

echo "[build] compiling llama.cpp for AMD EPYC..."
cd $LLAMA_DIR
cmake -B build-azure \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-O3 -march=x86-64-v3 -mno-avxvnni" \
    -DCMAKE_CXX_FLAGS="-O3 -march=x86-64-v3 -mno-avxvnni" \
    -DGGML_AVX2=ON \
    -DGGML_FMA=ON \
    -DGGML_AVX_VNNI=OFF \
    -DGGML_OPENMP=ON \
    -DBUILD_SHARED_LIBS=ON

cmake --build build-azure -j$(nproc)
strip build-azure/bin/llama-server
cd -

echo "[build] copying llama binaries into project..."
mkdir -p llama-bin/bin llama-bin/lib
cp $LLAMA_DIR/build-azure/bin/llama-server llama-bin/bin/
cp $LLAMA_DIR/build-azure/bin/*.so*        llama-bin/lib/ 2>/dev/null || true

echo "[build] compiling annealer for linux/amd64..."
cd $ANNEALER_DIR
make clean
CFLAGS="-O2 -march=x86-64-v3 -std=c11 -I./include" \
    CC=gcc make
strip annealer
cd -

echo "[build] building docker image..."
sudo docker build -t blackbox-annealer:latest .

echo "[build] done. test locally with:"
echo "  sudo docker run --rm blackbox-annealer:latest"

