add_executable(bootcupdatesd
    main.cpp
    updater.cpp
)

target_link_libraries(bootcupdatesd PRIVATE
    Qt6::Core
    Qt6::DBus
)

configure_file(systemd/bootcupdatesd.service.in bootcupdatesd.service @ONLY)
configure_file(dbus/io.github.jayelg.BootcUpdates1.service.in io.github.jayelg.BootcUpdates1.service @ONLY)

install(TARGETS bootcupdatesd DESTINATION ${KDE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bootcupdatesd.service DESTINATION lib/systemd/system)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.jayelg.BootcUpdates1.service DESTINATION ${KDE_INSTALL_DBUSDIR}/system-services)
install(FILES dbus/io.github.jayelg.BootcUpdates1.conf DESTINATION ${KDE_INSTALL_DBUSDIR}/system.d)
install(FILES polkit/io.github.jayelg.bootcupdates.policy DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)
