ARG buildtype=stable
FROM cyberway/cyberway:$buildtype as cyberway
FROM cyberway/cyberway.cdt:$buildtype as cdt

FROM cyberway/builder:$buildtype as builder
COPY --from=cdt /opt/cyberway.cdt /opt/cyberway.cdt
COPY --from=cyberway /opt/cyberway /opt/cyberway

ARG branch=master
ADD https://api.github.com/repos/GolosChain/golos.contracts/git/refs/heads/$branch /etc/version.json
RUN git clone -b $branch https://github.com/GolosChain/golos.contracts.git --recursive

RUN cd golos.contracts/cyberway.contracts \
    && echo ":$(git rev-parse HEAD)" > version \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/cyberway.contracts/ \
        -Deosio.cdt_DIR=/opt/cyberway.cdt/lib/cmake/eosio.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
    && cmake --build build --target install \
    && cd .. \
    && echo "$branch:$(git rev-parse HEAD)" > version \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/golos.contracts/ \
        -Deosio.cdt_DIR=/opt/cyberway.cdt/lib/cmake/eosio.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
    && cmake --build build --target install


FROM ubuntu:18.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /golos.contracts/version /opt/golos.contracts/version
COPY --from=builder /opt/golos.contracts/ /opt/golos.contracts/
COPY --from=builder /golos.contracts/build/tests/unit_test /opt/golos.contracts/unit_test

COPY --from=builder /golos.contracts/cyberway.contracts/version /opt/cyberway.contracts/version
COPY --from=builder /opt/cyberway.contracts/ /opt/cyberway.contracts/
COPY --from=builder /golos.contracts/cyberway.contracts/build/tests/unit_test /opt/cyberway.contracts/unit_test
COPY --from=builder /golos.contracts/cyberway.contracts/tests/test_contracts /opt/cyberway.contracts/test_contracts

COPY --from=builder /usr/local/lib/libbson-1.0.so* /usr/local/lib/

ENV CYBERWAY_TEST_CONTRACTS /opt/cyberway.contracts/test_contracts
ENV CYBERWAY_CONTRACTS /opt/cyberway.contracts
ENV GOLOS_CONTRACTS /opt/golos.contracts
