#!/bin/bash
set -e

: ${DEST:="genesis-data-temp"}
: ${GOLOS_STATE:="golos.dat"}
: ${GOLOS_OP_STATE:="operation_dump"}
: ${GOLOS_IMAGE:="cyberway/golos.contracts:latest"}

INITIAL_TIMESTAMP=$(date +"%FT%T.%3N" --utc)

echo "Create directory $DEST"
[ -d $DEST ] || mkdir $DEST

echo "Info about images $GOLOS_IMAGE:"
docker image ls $GOLOS_IMAGE | tail -n +2

LS_CMD="ls -l / /opt/cyberway/bin/ /opt/golos.contracts /opt/golos.contracts/genesis"
CREATE_GENESIS_CMD="/opt/cyberway/bin/create-genesis -g genesis-info.json -o /genesis-data/genesis.dat"
EE_OP_STATE_LINK=

if [ -d "$GOLOS_OP_STATE" ]; then
	LS_CMD="$LS_CMD /operation_dump"
	CREATE_GENESIS_CMD="$CREATE_GENESIS_CMD -e /genesis-data/event-genesis/ -d /operation_dump/"
	EE_OP_STATE_LINK="-v `readlink -f $GOLOS_OP_STATE`:/operation_dump"
fi

docker run --rm \
    -v `readlink -f $GOLOS_STATE`:/golos.dat \
    -v `readlink -f $GOLOS_STATE.map`:/golos.dat.map \
    -v `readlink -f $GOLOS_STATE.reputation`:/golos.dat.reputation \
    $EE_OP_STATE_LINK \
    -v `readlink -f $DEST`:/genesis-data \
    -e LS_CMD="$LS_CMD" \
    -e CREATE_GENESIS_CMD="$CREATE_GENESIS_CMD" \
    $GOLOS_IMAGE bash -c \
    ' $LS_CMD &&
     sed "s|\${INITIAL_TIMESTAMP}|'$INITIAL_TIMESTAMP'|; /^#/d" /opt/golos.contracts/genesis/genesis.json.tmpl | tee genesis.json && \
     sed "s|\$CYBERWAY_CONTRACTS|$CYBERWAY_CONTRACTS|;s|\$GOLOS_CONTRACTS|$GOLOS_CONTRACTS|; /^#/d" /opt/golos.contracts/genesis/genesis-info.json.tmpl | tee genesis-info.json && \
      $CREATE_GENESIS_CMD && \
     sed "s|\${GENESIS_DATA_HASH}|$(sha256sum /genesis-data/genesis.dat | cut -f1 -d" ")|" genesis.json | tee /genesis-data/genesis.json'
