#!/bin/bash
set -euo pipefail

REVISION=$(git rev-parse HEAD)
MASTER_REVISION=$(git rev-parse origin/master)

pushd cyberway.contracts

CYBERWAY_CONTRACTS_REVISION=$(git rev-parse HEAD)

popd

TAGREF=${BUILDKITE_TAG:+"tags/$BUILDKITE_TAG"}
REF=${TAGREF:-"heads/$BUILDKITE_BRANCH"}

if [[ ${REVISION} == ${MASTER_REVISION} ]]; then
    BUILDTYPE="stable"
else
    BUILDTYPE="latest"
fi

CDT_TAG=${CDT_TAG:-$BUILDTYPE}
CW_TAG=${CW_TAG:-$BUILDTYPE}
BUILDER_TAG=${BUILDER_TAG:-$BUILDTYPE}
docker build -t cyberway/golos.contracts:${REVISION} --build-arg=cw_tag=${CW_TAG} --build-arg=cdt_tag=${CDT_TAG} --build-arg=cyberway_contracts_version=${CYBERWAY_CONTRACTS_REVISION}  --build-arg=builder_tag=${BUILDER_TAG} --build-arg=version=${REVISION} -f Docker/Dockerfile .
