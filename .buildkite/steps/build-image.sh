#!/bin/bash
set -euo pipefail

IMAGETAG=${BUILDKITE_BRANCH:-master}
BRANCHNAME=${BUILDKITE_BRANCH:-master}
REVISION=$(git rev-parse HEAD)

pushd cyberway.contracts

CYBERWAY_CONTRACTS_REVISION=$(git rev-parse HEAD)

popd

TAGREF=${BUILDKITE_TAG:+"tags/$BUILDKITE_TAG"}
REF=${TAGREF:-"heads/$BUILDKITE_BRANCH"}

if [[ "${IMAGETAG}" == "master" ]]; then
    BUILDTYPE="stable"
elif [[ "${IMAGETAG}" == "alfa" ]]; then
    BUILDTYPE="alfa"
else
    BUILDTYPE="latest"
fi

docker build -t cyberway/golos.contracts:${IMAGETAG} --build-arg buildtype=${BUILDTYPE} --build-arg=verison=${REVISION} --build-arg=cyberway_contracts_version=${CYBERWAY_CONTRACTS_REVISION}  -f Docker/Dockerfile .
