kcmutils_add_qml_kcm(kcm_bootcupdates SOURCES kcm.cpp)

target_compile_definitions(kcm_bootcupdates PRIVATE TRANSLATION_DOMAIN="kcm_bootcupdates")

target_link_libraries(kcm_bootcupdates PRIVATE
    bootcupdatescommon
    Qt6::DBus
    Qt6::Quick
    KF6::CoreAddons
    KF6::KCMUtilsQuick
    KF6::I18n
)
