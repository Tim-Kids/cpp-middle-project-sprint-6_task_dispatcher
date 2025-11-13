FROM ubuntu@sha256:79efa276fdefa2ee3911db29b0608f8c0561c347ec3f4d4139980d43b168d991

# Update package database and install essential tools
RUN apt update && apt install -y \
    python3 python3-pip pipx git less vim sudo \
    cmake make g++-15 clangd-19 gdb  nodejs npm libjemalloc-dev \
    libssl-dev openssl libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Update gcc/g++ version to 15
RUN ln -fs /usr/bin/g++-15 /usr/bin/g++ && \
    ln -fs /usr/bin/gcc-15 /usr/bin/gcc

# Arguments for user configuration
ARG USER=dev
ARG USER_GID=1001
ARG USER_UID=1001

RUN groupadd -g ${USER_GID} ${USER} && \
    useradd -m -u ${USER_UID} -g ${USER_GID} -s /bin/bash ${USER} && \
    echo "${USER} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Build and install GoogleTest (gtest)
RUN git clone https://github.com/google/googletest.git /tmp/googletest && \
    cmake -E make_directory /tmp/googletest/build && \
    cd /tmp/googletest/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && make install && \
    rm -rf /tmp/googletest

# Build and install Google Benchmark (requires gtest)
RUN git clone https://github.com/google/benchmark.git /tmp/benchmark && \
    cmake -E make_directory /tmp/benchmark/build && \
    cd /tmp/benchmark/build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_TESTING=OFF .. && \
    make -j$(nproc) && make install && \
    rm -rf /tmp/benchmark

# Install core Tree-sitter library
RUN git clone https://github.com/tree-sitter/tree-sitter.git /tmp/tree-sitter && \
    cd /tmp/tree-sitter && \
    make -j$(nproc) && \
    make install && \
    cd .. && \
    rm -rf /tmp/tree-sitter

# Install curl (if not already present from earlier steps), Rust, and Cargo(Rust package manager which is needed for tree-sitter-cli)
# using the official Rust installation script, and immediately use cargo to install tree-sitter-cli. If run 'export PATH="/root/.cargo/bin:${PATH}"'
# apart from the current RUN layer's command a 'cargo' package won't be available for the next command.
RUN apt update && apt install -y curl && \
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y && \
    export PATH="/root/.cargo/bin:${PATH}" && \     
    cargo install tree-sitter-cli --root /usr/local && \
    # Clean up apt cache to reduce image size
    rm -rf /var/lib/apt/lists/*
# The ENV PATH instruction is still useful for subsequent RUN commands and the final container shell.
ENV PATH="/root/.cargo/bin:${PATH}"

# Build and install the Python grammar parser library
RUN git clone https://github.com/tree-sitter/tree-sitter-python.git /tmp/tree-sitter-python && \
    cd /tmp/tree-sitter-python && \
    tree-sitter init-config && \
    tree-sitter generate && \
    gcc -shared -o libtree-sitter-python.so -Isrc src/parser.c -fPIC && \
    mkdir -p ~/.tree-sitter/bin && \
    ln -s $(pwd)/libtree-sitter-python.so ~/.tree-sitter/bin/ && \
    cd .. && \
    rm -rf /tmp/tree-sitter-python

# Configure Tree-sitter to look for parsers in the user's directory
RUN echo '{"parser-directories":["/"]}' > /root/.config/tree-sitter/config.json && \
    chmod 777 /root/.config/tree-sitter/config.json && \
    chmod 755 /root /root/.config /root/.config/tree-sitter && \
    chmod 644 /root/.config/tree-sitter/config.json


USER ${USER}

WORKDIR /home/${USER}

# Set the default command
CMD ["/bin/bash"]
