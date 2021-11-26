FROM registry.fedoraproject.org/fedora:35

# Runtime packages.
RUN dnf update -y \
    && dnf install -y \
        golang \
        python3-pip \
        make \
    && dnf clean all

# Developemnt tools.
COPY requirements.txt requirements.txt
RUN python3 -m pip install --upgrade pip \
    && python3 -m pip install --requirement requirements.txt
