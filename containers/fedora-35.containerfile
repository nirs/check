FROM registry.fedoraproject.org/fedora:35

# Runtime packages.
RUN dnf install -y \
        golang \
        python3-pip \
        make \
    && dnf clean all

# Install pytest via pip.
RUN python3 -m pip install --upgrade pip \
    && python3 -m pip install --upgrade pytest
