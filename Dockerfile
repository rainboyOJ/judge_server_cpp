# syntax=docker/dockerfile:1

FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's@//.*archive.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        libseccomp-dev \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSJUDGER_ENABLE_SECCOMP=ON \
    && cmake --build build --target judge_server -j"$(nproc)"

RUN mkdir -p /artifact/app/build /artifact/checker \
    && cp build/judge_server /artifact/app/build/judge_server \
    && cp -a config /artifact/app/config \
    && cp -a testData /artifact/app/testData \
    && if [ -f checker/output/fcmp2 ]; then install -m 0755 checker/output/fcmp2 /artifact/checker/fcmp2; fi

FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's@//.*archive.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        g++ \
        libseccomp2 \
        python3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 ojrun \
    && useradd --uid 10001 --gid 10001 --no-create-home --shell /usr/sbin/nologin ojrun \
    && mkdir -p /opt/boxtest /judge/checker

WORKDIR /opt/boxtest

COPY --from=builder /artifact/app/ /opt/boxtest/
COPY --from=builder /artifact/checker/ /judge/checker/

EXPOSE 8000

CMD ["./build/judge_server", "config/config.json"]
