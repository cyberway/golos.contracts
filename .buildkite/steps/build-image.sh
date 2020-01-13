#!/bin/bash
set -euo pipefail

REVISION=$(git rev-parse HEAD)

if [[ "${BUILDKITE_BRANCH}" == "master" ]]; then
    BUILDTYPE="stable"
else
    BUILDTYPE="latest"
fi

CDT_TAG=${CDT_TAG:-$BUILDTYPE}
CW_TAG=${CW_TAG:-$BUILDTYPE}
BUILDER_TAG=${BUILDER_TAG:-$BUILDTYPE}
SYSTEM_CONTRACTS_TAG=${SYSTEM_CONTRACTS_TAG:-$BUILDTYPE}

docker build -t cyberway/golos.contracts:${REVISION} --build-arg=cw_tag=${CW_TAG} --build-arg=cdt_tag=${CDT_TAG} --build-arg=system_contracts_tag=${SYSTEM_CONTRACTS_TAG} --build-arg=builder_tag=${BUILDER_TAG} --build-arg=version=${REVISION} -f Docker/Dockerfile .
