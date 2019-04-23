#!/bin/bash
set -e


: ${TAG:="latest"}
: ${DEST:="genesis-data-temp"}
: ${GOLOS_STATE:="golos.dat"}

INITIAL_TIMESTAMP=$(date +"%FT%T.%3N" -d0)
GOLOS_IMAGE=cyberway/golos.contracts:$TAG
GOLOS=golos.genesis

docker kill $GOLOS || true
docker rm $GOLOS || true

echo "Create directory $DEST"
[ -d $DEST ] || mkdir $DEST

echo "Info about images $GOLOS_IMAGE:"
docker image ls $GOLOS_IMAGE | tail -n +2

docker run --name $GOLOS cyberway/golos.contracts:$TAG sleep 300 &
sleep 1
docker ps
docker cp $GOLOS_STATE $GOLOS:/golos.dat
docker cp $GOLOS_STATE.map $GOLOS:/golos.dat.map

docker exec $GOLOS bash -c \
    'ls -l / /opt/cyberway/bin/ /opt/golos.contracts /opt/golos.contracts/genesis &&
     sed "s|\${INITIAL_TIMESTAMP}|'$INITIAL_TIMESTAMP'|" /opt/golos.contracts/genesis/genesis.json.tmpl | tee genesis.json && \
     sed "s|\$CYBERWAY_CONTRACTS|$CYBERWAY_CONTRACTS|;s|\$GOLOS_CONTRACTS|$GOLOS_CONTRACTS|; /^#/d" /opt/golos.contracts/genesis/genesis-info.json.tmpl | tee genesis-info.json && \
     /opt/cyberway/bin/create-genesis -g genesis-info.json -o genesis.dat && \
     sed -i "s|\${GENESIS_DATA_HASH}|$(sha256sum genesis.dat | cut -f1 -d" ")|" genesis.json && cat genesis.json'

docker cp $GOLOS:genesis.dat $DEST/genesis.dat
docker cp $GOLOS:genesis.json $DEST/genesis.json

docker kill $GOLOS && docker rm $GOLOS
