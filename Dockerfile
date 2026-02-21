FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages for cross-compilation and development
RUN apt update && apt install -y \
    build-essential \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    gdb-multiarch \
    python3 \
    parted \
    mtools \
    dosfstools \
    && rm -rf /var/lib/apt/lists/*    

WORKDIR /Neutron

CMD ["/bin/bash"]