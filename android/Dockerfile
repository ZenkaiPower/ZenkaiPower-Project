FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates curl git xz-utils python3 python3-pil libxml2 libzstd1 libtinfo6 gcc libc6-dev make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
