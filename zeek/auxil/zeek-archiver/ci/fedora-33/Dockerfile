FROM fedora:33

RUN dnf -y install \
    git \
    cmake \
    make \
    gcc-c++ \
  && dnf clean all && rm -rf /var/cache/dnf
