FROM registry.fedoraproject.org/fedora:44

RUN dnf install -y --setopt=install_weak_deps=False \
        gcc-c++ cmake ninja-build extra-cmake-modules \
        qt6-qtbase-devel qt6-qtdeclarative-devel \
        kf6-kcmutils-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
        kf6-kconfig-devel kf6-knotifications-devel \
        rpm-build systemd-rpm-macros \
    && dnf clean all
