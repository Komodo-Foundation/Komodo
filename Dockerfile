# Try Komodo without installing anything:
#   docker build -t komodo .
#   docker run --rm -it -v "$PWD":/work -w /work komodo            # run the project in the current directory
#   docker run --rm -it komodo komodo repl
FROM debian:12-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        nasm gcc libc6-dev libgc1 libgc-dev pkg-config g++ ca-certificates git \
    && rm -rf /var/lib/apt/lists/*
COPY . /opt/komodo
RUN ln -s /opt/komodo/komodo /usr/local/bin/komodo \
    && /opt/komodo/build-self.sh && komodo -V
CMD ["komodo", "-h"]
