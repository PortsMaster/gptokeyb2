# syntax=docker/dockerfile:1.7
FROM ubuntu:22.04

# Run with: docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile -t gptokeyb2-build . && docker create --name gptokeyb2_tmp gptokeyb2-build && docker cp gptokeyb2_tmp:/build/gptokeyb2/build/gptokeyb2 ./gptokeyb2 && docker rm gptokeyb2_tmp

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /build

# -------------------------------------------------
# Dependencies
# -------------------------------------------------
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ccache \
    libsdl2-dev \
    libevdev-dev \
    ca-certificates \
    patchelf \
 && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------
# ccache configuration
# -------------------------------------------------
ENV CCACHE_DIR=/root/.ccache
ENV CC="ccache gcc"
ENV CXX="ccache g++"

# Optional but recommended
RUN ccache --set-config=cache_dir=/root/.ccache && \
    ccache --set-config=max_size=5G

# -------------------------------------------------
# Fetch sources
# -------------------------------------------------
RUN git clone https://github.com/PortsMaster/gptokeyb2.git

# -------------------------------------------------
# Configure (cached)
# -------------------------------------------------
WORKDIR /build/gptokeyb2

COPY . /build/gptokeyb2/

RUN --mount=type=cache,target=/root/.ccache \
    cmake -S . -B build \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# -------------------------------------------------
# Build (cached)
# -------------------------------------------------
RUN --mount=type=cache,target=/root/.ccache \
    cmake  --build build -j$(nproc)

# -------------------------------------------------
# Prepare export (MATCHES GITHUB WORKFLOW)
# -------------------------------------------------
RUN cd build && \
    patchelf --replace-needed libinterpose.so libinterpose.aarch64.so gptokeyb2 && \
    patchelf --set-soname libinterpose.aarch64.so lib/libinterpose.so && \
    mv lib/libinterpose.so libinterpose.aarch64.so

RUN find /build/gptokeyb2/

# -------------------------------------------------
# Strip (final binary)
# -------------------------------------------------
#RUN strip /build/gptokeyb2/build/gptokeyb2

# -------------------------------------------------
# Verify
# -------------------------------------------------
#RUN file /build/gptokeyb2/build/gptokeyb2