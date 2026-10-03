FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DQUINTUM_BUILD_GUI=OFF \
    && cmake --build build --parallel 2 --target quintumd

FROM ubuntu:24.04 AS runtime

RUN useradd \
        --system \
        --create-home \
        --home-dir /var/lib/quintum \
        --shell /usr/sbin/nologin \
        quintum

COPY --from=build /src/build/quintumd /usr/local/bin/quintumd

RUN chmod 0755 /usr/local/bin/quintumd \
    && chown -R quintum:quintum /var/lib/quintum

USER quintum
WORKDIR /var/lib/quintum

VOLUME ["/var/lib/quintum"]
EXPOSE 38444/tcp

ENTRYPOINT ["/usr/local/bin/quintumd"]
CMD ["--testnet", "--network-only", "--datadir", "/var/lib/quintum", "--listen-port", "38444"]
