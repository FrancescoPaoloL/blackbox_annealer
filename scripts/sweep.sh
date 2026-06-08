#!/bin/bash
# sweep.sh — run annealer at multiple thresholds, one deploy per threshold
# Usage: ./sweep.sh <docker-username> <resource-group>
set -e

DOCKERUSER=${1:?usage: ./sweep.sh <docker-username> <resource-group>}
RG=${2:?usage: ./sweep.sh <docker-username> <resource-group>}
IMAGE="$DOCKERUSER/blackbox-annealer:latest"
CONTAINER="blackbox-annealer-sweep"
RESULTS_DIR="./sweep_results"
THRESHOLDS="0.3 0.4 0.5 0.6 0.7"
TIMEOUT=1800  # 30 min max per run

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo " blackbox_annealer threshold sweep"
echo " image:    $IMAGE"
echo " group:    $RG"
echo " thresholds: $THRESHOLDS"
echo "=========================================="

STEP=0
TOTAL=5

for T in $THRESHOLDS; do
    STEP=$((STEP+1))
    echo ""
    echo "------------------------------------------"
    echo " [$STEP/$TOTAL] threshold=$T — START"
    echo "------------------------------------------"

    echo "[sweep] deleting old container..."
    az container delete --resource-group "$RG" --name "$CONTAINER" --yes 2>/dev/null || true

    echo "[sweep] creating container with GUARDIAN_THRESHOLD=$T..."
    az container create \
        --resource-group "$RG" \
        --name "$CONTAINER" \
        --image "$IMAGE" \
        --cpu 4 --memory 6 \
        --os-type Linux \
        --restart-policy Never \
        --environment-variables GUARDIAN_THRESHOLD=$T \
        --output none
    echo "[sweep] container created, waiting for run to finish..."

    WAIT=0
    while true; do
        STATE=$(az container show \
            --resource-group "$RG" \
            --name "$CONTAINER" \
            --query "containers[0].instanceView.currentState.state" \
            -o tsv 2>/dev/null)
        if [ "$STATE" = "Terminated" ]; then
            echo "[sweep] run finished"
            break
        fi
        WAIT=$((WAIT+10))
        if [ $WAIT -gt $TIMEOUT ]; then
            echo "[sweep] TIMEOUT after ${TIMEOUT}s — moving on"
            break
        fi
        echo "[sweep] state=$STATE elapsed=${WAIT}s..."
        sleep 10
    done

    LOGFILE="$RESULTS_DIR/threshold_${T}.log"
    echo "[sweep] collecting logs -> $LOGFILE"
    az container logs --resource-group "$RG" --name "$CONTAINER" > "$LOGFILE"

    RESULT=$(grep -E "BYPASS|NO BYPASS" "$LOGFILE" | tail -1)
    echo " [$STEP/$TOTAL] threshold=$T — DONE => $RESULT"

done

echo ""
echo "=========================================="
echo " SUMMARY"
echo "=========================================="
for T in $THRESHOLDS; do
    LOGFILE="$RESULTS_DIR/threshold_${T}.log"
    if [ -f "$LOGFILE" ]; then
        RESULT=$(grep -E "BYPASS|NO BYPASS" "$LOGFILE" | tail -1)
        printf "  threshold=%-5s  %s\n" "$T" "$RESULT"
    else
        printf "  threshold=%-5s  no log\n" "$T"
    fi
done
echo "=========================================="

