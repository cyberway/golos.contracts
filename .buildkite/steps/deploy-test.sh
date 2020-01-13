#/bin/bash

set -euo pipefail

docker stop mongo || true
docker rm mongo || true
docker volume rm cyberway-mongodb-data || true
docker volume create --name=cyberway-mongodb-data

cd Docker

IMAGETAG=$(git rev-parse HEAD)

docker-compose up -d

# Run unit-tests
sleep 10s
docker run --network golos-tests_contracts-net -ti cyberway/golos.contracts:$IMAGETAG  /bin/bash -c 'export MONGO_URL=mongodb://mongo:27017; /opt/golos.contracts/unit_test -l message -r detailed'
result=$?

docker-compose down

exit $result
