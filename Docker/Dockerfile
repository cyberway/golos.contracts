ARG buildtype=stable
FROM cyberway/cyberway:$buildtype as cyberway
FROM cyberway/cyberway.cdt:$buildtype as cdt

FROM cyberway/builder:$buildtype as builder
COPY --from=cdt /opt/cyberway.cdt /opt/cyberway.cdt
COPY --from=cyberway /opt/cyberway /opt/cyberway

ARG branch=master
ARG ref=heads/$branch

ADD https://api.github.com/repos/cyberway/golos.contracts/git/refs/$ref /etc/version.json
RUN git clone -b $branch https://github.com/cyberway/golos.contracts.git --recursive

RUN cd golos.contracts/cyberway.contracts \
    && echo ":$(git rev-parse HEAD)" > version \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/cyberway.contracts/ \
        -Dcyberway.cdt_DIR=/opt/cyberway.cdt/lib/cmake/cyberway.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
    && cmake --build build --target install \
    && cd .. \
    && echo "$branch:$(git rev-parse HEAD)" > version \
    && cmake -H. -B"build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/golos.contracts/ \
        -Dcyberway.cdt_DIR=/opt/cyberway.cdt/lib/cmake/cyberway.cdt \
        -DEOSIO_ROOT=/opt/cyberway \
    && cmake --build build --target install \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.social/golos.social.abi ./golos.social/golos.social.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.memo/golos.memo.abi ./golos.memo/golos.memo.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.vesting/golos.vesting.abi ./golos.vesting/golos.vesting.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.referral/golos.referral.abi ./golos.referral/abi/golos.referral.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.ctrl/golos.ctrl.abi ./golos.ctrl/abi/golos.ctrl.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.emit/golos.emit.abi ./golos.emit/abi/golos.emit.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.charge/golos.charge.abi ./golos.charge/golos.charge.abi \
    && /opt/cyberway.cdt/bin/cyberway-abidiff /opt/golos.contracts/golos.publication/golos.publication.abi ./golos.publication/golos.publication.abi \
    && echo "abidiff check passed"


FROM ubuntu:18.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates python3 python3-numpy python3-bson libusb-1.0-0-dev libcurl4-gnutls-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /golos.contracts/version /opt/golos.contracts/version
COPY --from=builder /opt/golos.contracts/ /opt/golos.contracts/
COPY --from=builder /golos.contracts/build/tests/unit_test /opt/golos.contracts/unit_test

COPY --from=builder /golos.contracts/cyberway.contracts/version /opt/cyberway.contracts/version
COPY --from=builder /opt/cyberway.contracts/ /opt/cyberway.contracts/
COPY --from=builder /golos.contracts/cyberway.contracts/build/tests/unit_test /opt/cyberway.contracts/unit_test
COPY --from=builder /golos.contracts/cyberway.contracts/tests/test_contracts /opt/cyberway.contracts/test_contracts

COPY --from=builder /golos.contracts/cyberway.contracts/scripts/bios-boot-sequence /opt/cyberway.contracts/scripts/bios-boot-sequence
COPY --from=builder /golos.contracts/scripts /opt/golos.contracts/scripts
COPY --from=builder /golos.contracts/scripts/boot-sequence.py /opt/golos.contracts/scripts/boot-sequence.py
COPY --from=builder /golos.contracts/genesis /opt/golos.contracts/genesis

COPY --from=builder /usr/local/lib/libbson-1.0.so.0.0.0 /usr/local/lib/
COPY --from=cyberway /opt/cyberway/bin/cleos /opt/cyberway/bin/cleos
COPY --from=cyberway /opt/cyberway/bin/keosd /opt/cyberway/bin/keosd
COPY --from=cyberway /opt/cyberway/bin/create-genesis /opt/cyberway/bin/create-genesis

RUN ldconfig && ln -s ../cyberway.contracts /opt/golos.contracts/cyberway.contracts

ENV CYBERWAY_TEST_CONTRACTS /opt/cyberway.contracts/test_contracts/
ENV CYBERWAY_CONTRACTS /opt/cyberway.contracts/
ENV GOLOS_CONTRACTS /opt/golos.contracts/
