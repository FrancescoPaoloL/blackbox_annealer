#!/bin/bash
# -----------------------------------------------------------------------
# deploy.sh: push to ACR and deploy to ACI
# Usage: ./deploy.sh <registry-name> <resource-group>
# -----------------------------------------------------------------------
set -e

REGISTRY=${1:?usage: ./deploy.sh <registry-name> <resource-group>}
RG=${2:?usage: ./deploy.sh <registry-name> <resource-group>}
IMAGE="$REGISTRY.azurecr.io/blackbox-annealer:latest"
CONTAINER_NAME="blackbox-annealer"

echo "[deploy] logging into ACR..."
az acr login --name $REGISTRY

echo "[deploy] tagging and pushing image..."
docker tag blackbox-annealer:latest $IMAGE
docker push $IMAGE

echo "[deploy] deploying to ACI..."
az container create \
    --resource-group $RG \
    --name $CONTAINER_NAME \
    --image $IMAGE \
    --cpu 4 \
    --memory 6 \
    --os-type Linux \
    --registry-login-server "$REGISTRY.azurecr.io" \
    --registry-username $(az acr credential show -n $REGISTRY --query username -o tsv) \
    --registry-password $(az acr credential show -n $REGISTRY --query passwords[0].value -o tsv) \
    --restart-policy Never

echo "[deploy] container started. follow logs with:"
echo "  az container logs --resource-group $RG --name $CONTAINER_NAME --follow"
echo ""
echo "[deploy] when done, destroy with:"
echo "  az container delete --resource-group $RG --name $CONTAINER_NAME --yes"

