# QuantumChess: Stockfish 19 for the QuantumChess bots (GPLv3).
#
# The default image holds both executables used by the QuantumChess API
# (StockfishPool looks for these paths):
#   /usr/local/bin/stockfish       official sf_19 release, linux-x86-64-universal (standard variant)
#   /usr/local/bin/stockfish-wrap  this fork, built from src/ (QuantumChess variant, wrapped files)
#
#   docker build -t stockfish-quantumchess .
#   docker run --rm -i stockfish-quantumchess          # UCI on stdin/stdout (wrap fork)
#
# The "windows" target exports the Windows builds for local development
# (stockfish.exe, official release; stockfish-wrap.exe, AVX2):
#   docker build --target windows -o <dir> .
#
# The NNUE network is exported from the official binary, so nothing is
# downloaded besides the release archives (checksums pinned below) and the
# base images.

FROM debian:trixie-slim AS official
ARG SF_URL=https://github.com/official-stockfish/Stockfish/releases/download/sf_19
ARG SF_LINUX_SHA256=9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f
ARG SF_WINDOWS_SHA256=3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl unzip \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /dl
RUN curl -fsSLo linux.tar.gz "$SF_URL/stockfish-linux-x86-64-universal.tar.gz" \
    && curl -fsSLo windows.zip "$SF_URL/stockfish-windows-x86-64-universal.zip" \
    && echo "$SF_LINUX_SHA256  linux.tar.gz" | sha256sum -c - \
    && echo "$SF_WINDOWS_SHA256  windows.zip" | sha256sum -c - \
    && mkdir linux windows /out \
    && tar -xzf linux.tar.gz -C linux \
    && unzip -q windows.zip -d windows \
    && install -m 755 linux/stockfish/stockfish-linux-x86-64-universal /out/stockfish \
    && install -m 755 windows/stockfish/stockfish-windows-x86-64-universal.exe /out/stockfish.exe

# Fork sources with the network next to them (make build then skips the download)
FROM gcc:15 AS sources
WORKDIR /sf/src
COPY scripts /sf/scripts
COPY src /sf/src
COPY --from=official /out/stockfish /tmp/stockfish
RUN printf "export_net\nquit\n" | /tmp/stockfish > /dev/null && ls nn-*.nnue

# Smoke test: the rook on a1 also reaches h1, g1 and f1 through the edge, 18
# moves instead of 15 (bench cannot be used, some of its positions are illegal
# with wrapped files)
FROM sources AS build-linux
RUN make -j"$(nproc)" build ARCH=x86-64-universal EXE=stockfish-wrap > /dev/null \
    && printf "uci\nposition fen 4k3/8/8/8/8/8/8/R3K3 w - - 0 1\ngo perft 1\nquit\n" \
       | ./stockfish-wrap > /tmp/check \
    && grep -q "id name Stockfish 19 QuantumChess" /tmp/check \
    && grep -q "Nodes searched: 18" /tmp/check

FROM gcc:15 AS build-windows
RUN apt-get update && apt-get install -y --no-install-recommends g++-mingw-w64-x86-64-posix \
    && rm -rf /var/lib/apt/lists/*
COPY --from=sources /sf /sf
WORKDIR /sf/src
RUN make -j"$(nproc)" build ARCH=x86-64-avx2 COMP=mingw EXE=stockfish-wrap.exe > /dev/null

FROM scratch AS windows
COPY --from=official /out/stockfish.exe /
COPY --from=build-windows /sf/src/stockfish-wrap.exe /
COPY Copying.txt AUTHORS /

FROM debian:trixie-slim
COPY --from=official /out/stockfish /usr/local/bin/stockfish
COPY --from=build-linux /sf/src/stockfish-wrap /usr/local/bin/stockfish-wrap
COPY --chmod=644 Copying.txt AUTHORS /usr/share/doc/stockfish/
ENTRYPOINT ["stockfish-wrap"]
