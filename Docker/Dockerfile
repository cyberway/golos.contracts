ARG cw_tag=stable
ARG cdt_tag=stable
ARG system_contracts_tag=stable
ARG builder_tag=stable

FROM cyberway/cyberway:$cw_tag as cyberway
FROM cyberway/cyberway.cdt:$cdt_tag as cdt
FROM cyberway/cyberway.contracts:$system_contracts_tag as cyberway.contracts
FROM cyberway/builder:$builder_tag as builder

COPY --from=cdt /opt/cyberway.cdt /opt/cyberway.cdt
COPY --from=cyberway /opt/cyberway /opt/cyberway

COPY . ./golos.contracts

ARG ci_build=false

RUN cd golos.contracts \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/golos.contracts/ \
        -Dcyberway.cdt_DIR=/opt/cyberway.cdt/lib/cmake/cyberway.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
        -DABICHECK=$ci_build \
    && cmake --build build --target install


FROM ubuntu:18.04
ARG version=unknown

COPY --from=builder /opt/golos.contracts/ /opt/golos.contracts/
COPY --from=builder /golos.contracts/build/tests/unit_test /opt/golos.contracts/unit_test
COPY --from=builder /golos.contracts/scripts /opt/golos.contracts/scripts
COPY --from=builder /golos.contracts/scripts/boot-sequence.py /opt/golos.contracts/scripts/boot-sequence.py
COPY --from=builder /golos.contracts/genesis /opt/golos.contracts/genesis
COPY --from=builder /usr/local/lib/libbson-1.0.so.0.0.0 /usr/local/lib/

COPY --from=cyberway /opt/cyberway/bin/cleos /opt/cyberway/bin/cleos
COPY --from=cyberway /opt/cyberway/bin/keosd /opt/cyberway/bin/keosd
COPY --from=cyberway /opt/cyberway/bin/create-genesis /opt/cyberway/bin/create-genesis

COPY --from=cyberway.contracts /opt/cyberway.contracts/ /opt/cyberway.contracts/

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates python3 python3-numpy python3-bson libusb-1.0-0-dev libcurl4-gnutls-dev \
    && rm -rf /var/lib/apt/lists/* \
    && echo $version > /opt/golos.contracts/version \
    && ldconfig 

ENV CYBERWAY_CONTRACTS /opt/cyberway.contracts/
ENV GOLOS_CONTRACTS /opt/golos.contracts/
