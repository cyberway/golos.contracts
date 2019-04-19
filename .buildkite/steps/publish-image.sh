#!/bin/bash
set -euo pipefail

IMAGETAG=${BUILDKITE_BRANCH:-master}
BRANCHNAME=${BUILDKITE_BRANCH:-master}

docker images

docker login -u=$DHUBU -p=$DHUBP
docker push cyberway/golos.contracts:${IMAGETAG}

if [[ "${IMAGETAG}" == "master" ]]; then
    docker tag cyberway/golos.contracts:${IMAGETAG} cyberway/golos.contracts:stable
    docker push cyberway/golos.contracts:stable
fi

if [[ "${IMAGETAG}" == "develop" ]]; then
    docker tag cyberway/golos.contracts:${IMAGETAG} cyberway/golos.contracts:latest
    docker push cyberway/golos.contracts:latest
fi

if [[ "${IMAGETAG}" == "prod-io" ]]; then
    docker tag cyberway/golos.contracts:${IMAGETAG} cyberway/golos.contracts:prod-io
    docker push cyberway/golos.contracts:prod-io
fi
