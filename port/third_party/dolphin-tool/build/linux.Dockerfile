FROM ubuntu:22.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends g++-12 gcc-12 ninja-build python3-pip pkg-config git ca-certificates && rm -rf /var/lib/apt/lists/*
RUN python3 -m pip install --no-cache-dir cmake==3.31.6
ENV CC=gcc-12 CXX=g++-12
WORKDIR /source
ENTRYPOINT ["sh", "/recipes/linux.sh"]
