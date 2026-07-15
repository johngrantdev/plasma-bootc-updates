add_executable(bootcupdates-notifier
    main.cpp
    notifier.cpp
)

target_link_libraries(bootcupdates-notifier PRIVATE
    bootcupdatescommon
    Qt6::Gui
    Qt6::DBus
    KF6::ConfigCore
    KF6::I18n
    KF6::Notifications
)

configure_file(systemd/bootcupdates-notifier.service.in bootcupdates-notifier.service @ONLY)

install(TARGETS bootcupdates-notifier DESTINATION ${KDE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bootcupdates-notifier.service DESTINATION lib/systemd/user)
install(FILES systemd/90-falcos-bootc-updates.preset DESTINATION lib/systemd/user-preset)
install(FILES bootcupdates.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
