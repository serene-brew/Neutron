FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------------
# Base dependencies
# ------------------------------------------------------------------
RUN apt update && apt install -y \
    build-essential \
    qemu-system-arm \
    qemu-system-aarch64 \
    gdb-multiarch \
    python3 \
    parted \
    mtools \
    dosfstools \
    wget \
    xz-utils \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------------
# Install Arm GNU Toolchain (AArch64 bare-metal)
# ------------------------------------------------------------------
ENV TOOLCHAIN_VERSION=14.2.rel1
ENV TOOLCHAIN_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${TOOLCHAIN_VERSION}-x86_64-aarch64-none-elf.tar.xz
ENV TOOLCHAIN_DIR=/opt/aarch64-none-elf

RUN wget -q ${TOOLCHAIN_URL} -O toolchain.tar.xz \
    && mkdir -p ${TOOLCHAIN_DIR} \
    && tar -xf toolchain.tar.xz --strip-components=1 -C ${TOOLCHAIN_DIR} \
    && rm toolchain.tar.xz

# Add toolchain to PATH
ENV PATH="${TOOLCHAIN_DIR}/bin:${PATH}"

# ------------------------------------------------------------------
# Work directory
# ------------------------------------------------------------------
WORKDIR /Neutron

CMD ["/bin/bash"]
