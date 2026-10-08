FROM docker.io/jenkins/inbound-agent:latest-jdk21

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        gcc \
        g++ \
        make \
        git \
        curl \
        wget \
        ca-certificates \
        pkg-config \
        python3 \
        libcurl4-openssl-dev \
        file \
        autoconf \
        automake \
        bison \
        flex \
        gettext \
        rsync \
        libreadline-dev \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

#
# AmigaOS 3.x / Motorola 680x0 cross compiler
#
RUN git clone --depth=1 \
        https://github.com/AmigaPorts/m68k-amigaos-gcc.git \
        /tmp/amiga-gcc \
    && mkdir -p /opt/amiga \
    && chown -R jenkins:jenkins /opt/amiga /tmp/amiga-gcc

USER jenkins

RUN cd /tmp/amiga-gcc \
    && make update \
    && make all PREFIX=/opt/amiga -j2

ENV PATH="/opt/amiga/bin:${PATH}"

WORKDIR /home/jenkins/agent
