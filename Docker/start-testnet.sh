#!/bin/bash
set -e

function call_hook {
    [[ ! -x hooks.sh ]] || ./hooks.sh "$@"
}

script_path=$(dirname $(readlink -f $0))
[[ -e env.sh ]] &&
    echo "=== Load environments from env.sh" &&
    . ./env.sh

if [[ ! -f config.ini ]]; then
    echo "Missing config.ini file for nodeos in current directory"
    exit 1
fi

set ${REPOSITORY:=cyberway}
set ${BUILDTYPE:=latest}

[[ -x shutdown.sh ]] && (
    echo "=== Shutdown previous instance"
    ./shutdown.sh; rm shutdown.sh)

echo "=== Prepare scripts for manage testnet"
[[ -f shutdown.sh ]] && rm shutdown.sh
[[ -f down.sh ]] && rm down.sh
[[ -f up.sh ]] && rm up.sh

cat >shutdown.sh <<DONE
#!/bin/bash
startup/run-with-events.sh cleanup
exit 0
DONE

cat >down.sh <<DONE
#!/bin/bash
startup/run-with-events.sh down
exit 0
DONE

cat >up.sh <<DONE
#!/bin/bash 
startup/run-with-events.sh up
exit 0
DONE
chmod a+x shutdown.sh down.sh up.sh

call_hook "files-preextract"

CYBERWAY_IMAGE=$REPOSITORY/cyberway:$BUILDTYPE
echo "=== Extract cyberway startup scripts from $CYBERWAY_IMAGE"
rm -fR startup || true
docker create --name extract-cyberway $CYBERWAY_IMAGE
docker cp extract-cyberway:/opt/cyberway/startup startup
docker rm extract-cyberway

GOLOS_IMAGE=$REPOSITORY/golos.contracts:$BUILDTYPE
echo "=== Extract Golos convertion scripts from $GOLOS_IMAGE"
rm -fR scripts || true
docker create --name extract-golos-scripts $GOLOS_IMAGE
docker cp extract-golos-scripts:/opt/golos.contracts/scripts scripts
docker rm extract-golos-scripts
call_hook "files-extracted"

echo "=== Start cyberway containers"
startup/run-with-events.sh

echo "=== Waiting for nodeosd started"
docker run --rm --network cyberway_cyberway-net -ti $GOLOS_IMAGE /bin/bash -c 'retry=30; until /opt/cyberway/bin/cleos --url http://nodeosd:8888 get info; do sleep 10; let retry--; [ $retry -gt 0 ] || exit 1; done; exit 0'
call_hook "nodeos-started"

echo "=== Deploy cyberway & golos contracts"
docker run --rm --network cyberway_cyberway-net -ti $GOLOS_IMAGE /opt/golos.contracts/scripts/boot-sequence.py
call_hook "contracts-loaded"

if [[ -n "$GOLOS_DB" ]]; then
    [[ -z "$CYBERWAY_DB" ]] && export CYBERWAY_DB=$(awk -F'[=#]' '/^chaindb_address\s*=/{sub(/^ *| *$/, "", $2); print $2;}' <${PWD}/config.ini)
    echo "=== Convert data from Golos database $GOLOS_DB into $CYBERWAY_DB"
    call_hook "convert-prepare"
    time scripts/convert.py
    if [[ -z "$SKIP_CONVERT_PUBLISH" ]]; then
       time scripts/convert_publish.py
       time scripts/update_childcount.py
    fi
    call_hook "convert-finish"
    docker stop nodeosd
    docker stop cyberway-notifier
    docker start cyberway-notifier nodeosd
else
    call_hook "convert-skipped"
fi

call_hook "testnet-started"
echo "Testnet deployed successfully!!!"
