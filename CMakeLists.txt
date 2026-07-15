cmake_minimum_required(VERSION 3.24)
project(falcos-bootc-updates VERSION 0.1.0)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Concurrent Core DBus Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons KCMUtils I18n Notifications)

add_subdirectory(src/daemon)
add_subdirectory(src/kcm)
add_subdirectory(src/notifier)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
