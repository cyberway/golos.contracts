#!/bin/bash
set -euo pipefail

REVISION=$(git rev-parse HEAD)
MASTER_REVISION=$(git rev-parse origin/master)
DEVELOP_REVISION=$(git rev-parse origin/develop)

docker images

docker login -u=$DHUBU -p=$DHUBP

if [[ "${REVISISON}" == "${MASTER_REVISION}" ]]; then
    docker tag cyberway/golos.contracts:${REVISION} cyberway/golos.contracts:stable
    docker push cyberway/golos.contracts:stable
fi

if [[ "${REVISION}" == "${DEVELOP_REVISION}" ]]; then
    docker tag cyberway/golos.contracts:${REVISION} cyberway/golos.contracts:latest
    docker push cyberway/golos.contracts:latest
fi

